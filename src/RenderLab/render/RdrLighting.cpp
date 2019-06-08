#include "Precompiled.h"
#include "RdrLighting.h"
#include "RdrFrameMem.h"
#include "Renderer.h"
#include "Scene.h"
#include "RdrComputeOp.h"
#include "RdrOffscreenTasks.h"
#include "components/Light.h"
#include "components/SkyVolume.h"
#include "Entity.h"

namespace
{
	static uint s_shadowMapSize = 2048;
	static uint s_shadowCubeMapSize = 512;

	struct ShadowMapData
	{
		Matrix44 mtxViewProj;
		float partitionEndZ; // Directional light only
	};

	Matrix44 getLightViewMatrix(const Vec3& pos, const Vec3& dir)
	{
		Vec3 upDir = Vec3::kUnitY;
		if (dir.y > 0.99f)
			upDir = -Vec3::kUnitZ;
		else if (dir.y < -0.99f)
			upDir = Vec3::kUnitZ;

		return Matrix44LookToLH(pos, dir, upDir);
	}

	void accumQuadMinMax(Quad& quad, Vec3& boundsMin, Vec3& boundsMax)
	{
		boundsMin = Vec3Min(boundsMin, quad.bottomLeft);
		boundsMin = Vec3Min(boundsMin, quad.bottomRight);
		boundsMin = Vec3Min(boundsMin, quad.topLeft);
		boundsMin = Vec3Min(boundsMin, quad.topRight);

		boundsMax = Vec3Max(boundsMax, quad.bottomLeft);
		boundsMax = Vec3Max(boundsMax, quad.bottomRight);
		boundsMax = Vec3Max(boundsMax, quad.topLeft);
		boundsMax = Vec3Max(boundsMax, quad.topRight);
	}
}


void RdrLightList::AddLight(const Light* pLight)
{
	switch (pLight->GetType())
	{
	case LightType::Directional:
		{
			DirectionalLight light;
			light.direction = pLight->GetDirection();
			light.color = pLight->GetColor();
			light.pssmLambda = pLight->GetPssmLambda();
			light.shadowMapIndex = -1;
			m_directionalLights.push(light);
		}
		break;
	case LightType::Point:
		{
			PointLight light;
			light.position = pLight->GetEntity()->GetPosition();
			light.color = pLight->GetColor();
			light.radius = pLight->GetRadius();
			light.shadowMapIndex = -1;
			m_pointLights.push(light);
		}
		break;
	case LightType::Spot:
		{
			SpotLight light;
			light.position = pLight->GetEntity()->GetPosition();
			light.direction = pLight->GetDirection();
			light.color = pLight->GetColor();
			light.radius = pLight->GetRadius();
			light.innerConeAngleCos = pLight->GetInnerConeAngleCos();
			light.outerConeAngle = pLight->GetOuterConeAngle();
			light.outerConeAngleCos = pLight->GetOuterConeAngleCos();
			light.shadowMapIndex = -1;
			m_spotLights.push(light);
		}
		break;
	case LightType::Environment:
		{
			EnvironmentLight light;
			light.position = pLight->GetEntity()->GetPosition();
			light.environmentMapIndex = pLight->GetEnvironmentTextureIndex();

			if (pLight->IsGlobalEnvironmentLight())
			{
				m_globalEnvironmentLight = light;
			}
			else
			{
				m_environmentLights.push(light);
			}
		}
	break;
	}
}

void RdrLightList::Clear()
{
	m_directionalLights.clear();
	m_pointLights.clear();
	m_spotLights.clear();
	m_environmentLights.clear();
	m_globalEnvironmentLight.environmentMapIndex = -1;
}

//////////////////////////////////////////////////////////////////////////
// RdrLighting

RdrLighting::RdrLighting()
	: m_hFogConstants(0)
	, m_hFogDensityLightLut(0)
	, m_hFogFinalLut(0)
	, m_fogLutSize(0, 0, 0)
{

}

void RdrLighting::Cleanup()
{
	RdrResourceCommandList& rResCommands = g_pRenderer->GetResourceCommandList();

	for (uint i = 0; i < MAX_SHADOW_MAPS; ++i)
	{
		if (m_shadowMapDepthViews[i])
		{
			rResCommands.ReleaseDepthStencilView(m_shadowMapDepthViews[i], CREATE_BACKPOINTER(this));
			m_shadowMapDepthViews[i] = 0;
		}
	}

	uint numCubemapViews = MAX_SHADOW_CUBEMAPS * (uint)CubemapFace::Count;
	for (uint i = 0; i < numCubemapViews; ++i)
	{
		if (m_shadowCubeMapDepthViews[i])
		{
			rResCommands.ReleaseDepthStencilView(m_shadowCubeMapDepthViews[i], CREATE_BACKPOINTER(this));
			m_shadowCubeMapDepthViews[i] = 0;
		}
	}

	if (m_hShadowMapTexArray)
	{
		rResCommands.ReleaseResource(m_hShadowMapTexArray, CREATE_BACKPOINTER(this));
		m_hShadowMapTexArray = 0;
	}

	if (m_hShadowCubeMapTexArray)
	{
		rResCommands.ReleaseResource(m_hShadowCubeMapTexArray, CREATE_BACKPOINTER(this));
		m_hShadowCubeMapTexArray = 0;
	}
}

void RdrLighting::LazyInitShadowResources()
{
	if (!m_hShadowMapTexArray)
	{
		m_hShadowMapTexArray = RdrResourceSystem::CreateTexture2DArray(s_shadowMapSize, s_shadowMapSize, MAX_SHADOW_MAPS,
			kDefaultDepthFormat, RdrResourceAccessFlags::CpuRO_GpuRO_RenderTarget, CREATE_BACKPOINTER(this));
		for (int i = 0; i < MAX_SHADOW_MAPS; ++i)
		{
			m_shadowMapDepthViews[i] = RdrResourceSystem::CreateDepthStencilView(m_hShadowMapTexArray, i, 1, CREATE_BACKPOINTER(this));
		}

		m_hShadowCubeMapTexArray = RdrResourceSystem::CreateTextureCubeArray(s_shadowCubeMapSize, s_shadowCubeMapSize, MAX_SHADOW_CUBEMAPS,
			kDefaultDepthFormat, RdrResourceAccessFlags::CpuRO_GpuRO_RenderTarget, CREATE_BACKPOINTER(this));

		for (int i = 0; i < MAX_SHADOW_CUBEMAPS * (int)CubemapFace::Count; ++i)
		{
			m_shadowCubeMapDepthViews[i] = RdrResourceSystem::CreateDepthStencilView(m_hShadowCubeMapTexArray, i, 1, CREATE_BACKPOINTER(this));
		}
	}
}

void RdrLighting::QueueDraw(RdrAction* pAction, RdrLightList* pLights, RdrLightingMethod lightingMethod,
	const AssetLib::VolumetricFogSettings& rFog, const float sceneDepthMin, const float sceneDepthMax,
	RdrActionLightResources* pOutResources)
{
	const Camera& rCamera = pAction->GetCamera();
	Camera lightCamera;
	int curShadowMapIndex = 0;
	int curShadowCubeMapIndex = 0;
	int shadowMapsThisFrame = 0;

	LazyInitShadowResources();

	// Update shadow casting lights and the global light constants
	// todo: Choose shadow lights based on location and dynamic object movement
	GlobalLightData* pGlobalLights = (GlobalLightData*)RdrFrameMem::AllocAligned(sizeof(GlobalLightData), 16);

	for (DirectionalLight& rDirLight : pLights->m_directionalLights)
	{
		int remainingShadowMaps = MAX_SHADOW_MAPS - curShadowMapIndex - 1;
		const int kNumCascades = 4;
		if (remainingShadowMaps >= kNumCascades)
		{
			// Directional lights always change because they're based on camera position/rotation
			rDirLight.shadowMapIndex = curShadowMapIndex;

			Rect viewport(0.f, 0.f, (float)s_shadowMapSize, (float)s_shadowMapSize);
			float zMin = sceneDepthMin;
			float zMax = sceneDepthMax;
			float zDiff = (zMax - zMin);

			float nearDepth = zMin;
			float lamda = rDirLight.pssmLambda;

			for (int iPartition = 0; iPartition < kNumCascades; ++iPartition)
			{
				float zUni = zMin + (zDiff / kNumCascades) * (iPartition + 1);
				float zLog = zMin * powf(zMax / zMin, (iPartition + 1) / (float)kNumCascades);
				float farDepth = lamda * zLog + (1.f - lamda) * zUni;

				Vec3 center = rCamera.GetPosition() + rCamera.GetDirection() * ((farDepth + nearDepth) * 0.5f);
				Matrix44 lightViewMtx = getLightViewMatrix(center, rDirLight.direction);

				Quad nearQuad = rCamera.GetFrustumQuad(nearDepth);
				QuadTransform(nearQuad, lightViewMtx);

				Quad farQuad = rCamera.GetFrustumQuad(farDepth);
				QuadTransform(farQuad, lightViewMtx);

				Vec3 boundsMin(FLT_MAX, FLT_MAX, FLT_MAX);
				Vec3 boundsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
				accumQuadMinMax(nearQuad, boundsMin, boundsMax);
				accumQuadMinMax(farQuad, boundsMin, boundsMax);

				float height = (boundsMax.y - boundsMin.y);
				float width = (boundsMax.x - boundsMin.x);
				lightCamera.SetAsOrtho(center, rDirLight.direction, std::max(width, height), -500.f, 500.f);

				pAction->QueueShadowMapPass(lightCamera, m_shadowMapDepthViews[curShadowMapIndex], viewport);

				Matrix44 mtxView;
				Matrix44 mtxProj;
				lightCamera.GetMatrices(mtxView, mtxProj);
				pGlobalLights->shadowData[curShadowMapIndex].mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
				pGlobalLights->shadowData[curShadowMapIndex].mtxViewProj = Matrix44Transpose(pGlobalLights->shadowData[curShadowMapIndex].mtxViewProj);
				pGlobalLights->shadowData[curShadowMapIndex].partitionEndZ = (iPartition == kNumCascades - 1) ? FLT_MAX : farDepth;

				++shadowMapsThisFrame;
				++curShadowMapIndex;
				nearDepth = farDepth;
			}
		}
		else
		{
			rDirLight.shadowMapIndex = -1;
		}
	}

	for (SpotLight& rSpotLight : pLights->m_spotLights)
	{
		int remainingShadowMaps = MAX_SHADOW_MAPS - curShadowMapIndex - 1;
		if (remainingShadowMaps)
		{
			Matrix44 mtxView;
			Matrix44 mtxProj;

			float angle = rSpotLight.outerConeAngle + Maths::DegToRad(5.f);
			lightCamera.SetAsPerspective(rSpotLight.position, rSpotLight.direction, angle * 2.f, 1.f, 0.1f, 1000.f);
			lightCamera.GetMatrices(mtxView, mtxProj);

			pGlobalLights->shadowData[curShadowMapIndex].mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
			pGlobalLights->shadowData[curShadowMapIndex].mtxViewProj = Matrix44Transpose(pGlobalLights->shadowData[curShadowMapIndex].mtxViewProj);

			Rect viewport(0.f, 0.f, (float)s_shadowMapSize, (float)s_shadowMapSize);
			pAction->QueueShadowMapPass(lightCamera, m_shadowMapDepthViews[curShadowMapIndex], viewport);

			rSpotLight.shadowMapIndex = curShadowMapIndex;
			++shadowMapsThisFrame;
			++curShadowMapIndex;
		}
		else
		{
			rSpotLight.shadowMapIndex = -1;
		}
	}

	for (PointLight& rPointLight : pLights->m_pointLights)
	{
		int remainingShadowCubeMaps = MAX_SHADOW_CUBEMAPS - curShadowCubeMapIndex - 1;
		if (remainingShadowCubeMaps)
		{
			rPointLight.shadowMapIndex = curShadowCubeMapIndex + MAX_SHADOW_MAPS;

			Rect viewport(0.f, 0.f, (float)s_shadowCubeMapSize, (float)s_shadowCubeMapSize);
			for (uint face = 0; face < 6; ++face)
			{
				lightCamera.SetAsCubemapFace(rPointLight.position, (CubemapFace)face, 0.1f, rPointLight.radius * 2.f);
				pAction->QueueShadowMapPass(lightCamera, m_shadowCubeMapDepthViews[curShadowCubeMapIndex * (int)CubemapFace::Count + face], viewport);
			}

			++shadowMapsThisFrame;
			++curShadowCubeMapIndex;
		}
		else
		{
			rPointLight.shadowMapIndex = -1;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Apply resource updates
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetResourceCommandList();
	RdrActionLightResources& outResources = *pOutResources;

	// Update spot lights
	SpotLight* pSpotLights = (SpotLight*)RdrFrameMem::Alloc(sizeof(SpotLight) * pLights->m_spotLights.size());
	memcpy(pSpotLights, pLights->m_spotLights.getData(), sizeof(SpotLight) * pLights->m_spotLights.size());
	if (!outResources.hSpotLightListRes)
		outResources.hSpotLightListRes = RdrResourceSystem::CreateStructuredBuffer(pSpotLights, pLights->m_spotLights.capacity(), sizeof(SpotLight), RdrResourceAccessFlags::CpuRW_GpuRO, CREATE_BACKPOINTER(this));
	else
		rResCommandList.UpdateBuffer(outResources.hSpotLightListRes, pSpotLights, sizeof(SpotLight) * pLights->m_spotLights.size(), CREATE_BACKPOINTER(this));

	// Update point lights
	PointLight* pPointLights = (PointLight*)RdrFrameMem::Alloc(sizeof(PointLight) * pLights->m_pointLights.size());
	memcpy(pPointLights, pLights->m_pointLights.getData(), sizeof(PointLight) * pLights->m_pointLights.size());
	if (!outResources.hPointLightListRes)
		outResources.hPointLightListRes = RdrResourceSystem::CreateStructuredBuffer(pPointLights, pLights->m_pointLights.capacity(), sizeof(PointLight), RdrResourceAccessFlags::CpuRW_GpuRO, CREATE_BACKPOINTER(this));
	else
		rResCommandList.UpdateBuffer(outResources.hPointLightListRes, pPointLights, sizeof(PointLight) * pLights->m_pointLights.size(), CREATE_BACKPOINTER(this));

	// Update environment lights
	EnvironmentLight* pEnvironmentLights = (EnvironmentLight*)RdrFrameMem::Alloc(sizeof(EnvironmentLight) * pLights->m_environmentLights.size());
	memcpy(pEnvironmentLights, pLights->m_environmentLights.getData(), sizeof(EnvironmentLight) * pLights->m_environmentLights.size());
	if (!outResources.hEnvironmentLightListRes)
		outResources.hEnvironmentLightListRes = RdrResourceSystem::CreateStructuredBuffer(pEnvironmentLights, pLights->m_environmentLights.capacity(), sizeof(EnvironmentLight), RdrResourceAccessFlags::CpuRW_GpuRO, CREATE_BACKPOINTER(this));
	else
		rResCommandList.UpdateBuffer(outResources.hEnvironmentLightListRes, pEnvironmentLights, sizeof(EnvironmentLight) * pLights->m_environmentLights.size(), CREATE_BACKPOINTER(this));

	// Light culling
	RdrResourceHandle hLightIndicesRes;
	if (lightingMethod == RdrLightingMethod::Clustered)
	{
		QueueClusteredLightCulling(pAction, rCamera, pLights->m_spotLights.size(), pLights->m_pointLights.size(), pOutResources);
		hLightIndicesRes = m_clusteredLightData.hLightIndices;
	}
	else
	{
		QueueTiledLightCulling(pAction, rCamera, pLights->m_spotLights.size(), pLights->m_pointLights.size());
		hLightIndicesRes = m_tiledLightData.hLightIndices;
	}

	// Update global lights constant buffer last so the light culling data is up to date.
	pGlobalLights->numDirectionalLights = pLights->m_directionalLights.size();
	pGlobalLights->globalEnvironmentLight = pLights->m_globalEnvironmentLight;
	pGlobalLights->clusterTileSize = m_clusteredLightData.clusterTileSize;
	pGlobalLights->volumetricFogFarDepth = rFog.farDepth;
	memcpy(pGlobalLights->directionalLights, pLights->m_directionalLights.getData(), sizeof(DirectionalLight) * pGlobalLights->numDirectionalLights);
	outResources.hGlobalLightsCb = RdrResourceSystem::CreateUpdateConstantBuffer(outResources.hGlobalLightsCb,
		pGlobalLights, sizeof(GlobalLightData), RdrResourceAccessFlags::CpuRW_GpuRO, CREATE_BACKPOINTER(this));

	//////////////////////////////////////////////////////////////////////////
	// Volumetric fog
	RdrResourceHandle hFogLut = 0;
	if (rFog.enabled)
	{
		QueueVolumetricFog(pAction, rFog, hLightIndicesRes);
		hFogLut = m_hFogFinalLut;
	}
	else
	{
		hFogLut = RdrResourceSystem::GetDefaultResourceHandle(RdrDefaultResource::kBlackTex3d);
	}

	// Fill out resource params
	RdrSharedLightResources& sharedResources = outResources.sharedResources;
	sharedResources.hVolumetricFogLut = hFogLut;
	sharedResources.hShadowCubeMapTexArray = m_hShadowCubeMapTexArray;
	sharedResources.hShadowMapTexArray = m_hShadowMapTexArray;
	sharedResources.hLightIndicesRes = hLightIndicesRes;
}

void RdrLighting::QueueClusteredLightCulling(RdrAction* pAction, const Camera& rCamera, int numSpotLights, int numPointLights,
	RdrActionLightResources* pOutResources)
{
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetResourceCommandList();
	const Rect& viewport = pAction->GetViewport();

	// Determine the cluster tile size given two rules:
	//	1) The larger viewport dimension will have 16 tiles.
	//	2) Tile size must be a multiple of 8.
	const uint kMaxTilesPerDimension = 16;
	float maxDim = max(viewport.width, viewport.height);
	m_clusteredLightData.clusterTileSize = ((uint)ceil(maxDim / kMaxTilesPerDimension) + 7) & ~7;

	int clusterCountX = (int)ceil(viewport.width / m_clusteredLightData.clusterTileSize);
	int clusterCountY = (int)ceil(viewport.height / m_clusteredLightData.clusterTileSize);

	if (m_clusteredLightData.clusterCountX < clusterCountX || m_clusteredLightData.clusterCountY < clusterCountY)
	{
		// Resize buffers
		m_clusteredLightData.clusterCountX = max(16, clusterCountX);
		m_clusteredLightData.clusterCountY = max(16, clusterCountY);

		if (m_clusteredLightData.hLightIndices)
			rResCommandList.ReleaseResource(m_clusteredLightData.hLightIndices, CREATE_BACKPOINTER(this));
		m_clusteredLightData.hLightIndices = RdrResourceSystem::CreateDataBuffer(nullptr
			, m_clusteredLightData.clusterCountX * m_clusteredLightData.clusterCountY * CLUSTEREDLIGHTING_DEPTH_SLICES * CLUSTEREDLIGHTING_BLOCK_SIZE
			, RdrResourceFormat::R16_UINT, RdrResourceAccessFlags::GpuRW
			, CREATE_BACKPOINTER(this));
	}

	RdrComputeOp* pCullOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
	pCullOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::ClusteredLightCull);
	pCullOp->threads[0] = clusterCountX;
	pCullOp->threads[1] = clusterCountY;
	pCullOp->threads[2] = CLUSTEREDLIGHTING_DEPTH_SLICES;

	pCullOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_clusteredLightData.hLightIndices, 1, CREATE_BACKPOINTER(this));
	
	RdrResourceHandle ahResources[] = { pOutResources->hSpotLightListRes, pOutResources->hPointLightListRes };
	pCullOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));

	uint constantsSize = sizeof(ClusteredLightCullingParams);
	ClusteredLightCullingParams* pParams = (ClusteredLightCullingParams*)RdrFrameMem::AllocAligned(constantsSize, 16);

	Matrix44 mtxProj;
	rCamera.GetMatrices(pParams->mtxView, mtxProj);

	pParams->mtxView = Matrix44Transpose(pParams->mtxView);
	pParams->proj_11 = mtxProj._11;
	pParams->proj_22 = mtxProj._22;
	pParams->screenSize = Vec2(viewport.width, viewport.height);
	pParams->fovY = rCamera.GetFieldOfViewY();
	pParams->aspectRatio = rCamera.GetAspectRatio();
	pParams->clusterCountX = clusterCountX;
	pParams->clusterCountY = clusterCountY;
	pParams->clusterCountZ = CLUSTEREDLIGHTING_DEPTH_SLICES;
	pParams->clusterTileSize = m_clusteredLightData.clusterTileSize;
	pParams->spotLightCount = numSpotLights;
	pParams->pointLightCount = numPointLights;

	RdrConstantBufferHandle hCullConstants = RdrResourceSystem::CreateTempConstantBuffer(pParams, constantsSize, CREATE_BACKPOINTER(this));
	pCullOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(&hCullConstants, 1, CREATE_BACKPOINTER(this));

	pAction->AddComputeOp(pCullOp, RdrPass::LightCulling);
}

void RdrLighting::QueueTiledLightCulling(RdrAction* pAction, const Camera& rCamera, int numSpotLights, int numPointLights)
{
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetResourceCommandList();
	const RdrActionLightResources& rLightParams = pAction->GetLightResources();
	const Rect& viewport = pAction->GetViewport();

	uint tileCountX = RdrComputeOp::getThreadGroupCount((uint)viewport.width, TILEDLIGHTING_TILE_SIZE);
	uint tileCountY = RdrComputeOp::getThreadGroupCount((uint)viewport.height, TILEDLIGHTING_TILE_SIZE);
	bool tileCountChanged = false;

	if (m_tiledLightData.tileCountX != tileCountX || m_tiledLightData.tileCountY != tileCountY)
	{
		m_tiledLightData.tileCountX = tileCountX;
		m_tiledLightData.tileCountY = tileCountY;
		tileCountChanged = true;
	}

	//////////////////////////////////////
	// Depth min max
	if (tileCountChanged)
	{
		if (m_tiledLightData.hDepthMinMaxTex)
			rResCommandList.ReleaseResource(m_tiledLightData.hDepthMinMaxTex, CREATE_BACKPOINTER(this));
		m_tiledLightData.hDepthMinMaxTex = RdrResourceSystem::CreateTexture2D(tileCountX, tileCountY, RdrResourceFormat::R16G16_FLOAT,
			RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));
	}

	// Update constants
	Matrix44 viewMtx;
	Matrix44 invProjMtx;
	rCamera.GetMatrices(viewMtx, invProjMtx);
	invProjMtx = Matrix44Inverse(invProjMtx);
	invProjMtx = Matrix44Transpose(invProjMtx);

	uint constantsSize = sizeof(Vec4) * 4;
	Vec4* pConstants = (Vec4*)RdrFrameMem::AllocAligned(constantsSize, 16);
	for (int i = 0; i < 4; ++i)
	{
		pConstants[i].x = invProjMtx.m[i][0];
		pConstants[i].y = invProjMtx.m[i][1];
		pConstants[i].z = invProjMtx.m[i][2];
		pConstants[i].w = invProjMtx.m[i][3];
	}

	RdrConstantBufferHandle hDepthMinMaxConstants = RdrResourceSystem::CreateTempConstantBuffer(pConstants, constantsSize, CREATE_BACKPOINTER(this));

	// Fill draw op
	{
		RdrComputeOp* pDepthOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
		pDepthOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::TiledDepthMinMax);
		pDepthOp->threads[0] = tileCountX;
		pDepthOp->threads[1] = tileCountY;
		pDepthOp->threads[2] = 1;
		pDepthOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_tiledLightData.hDepthMinMaxTex, 1, CREATE_BACKPOINTER(this));

		RdrResourceHandle ahResources[] = { pAction->GetPrimaryDepthBuffer() };
		pDepthOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));
		pDepthOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(&hDepthMinMaxConstants, 1, CREATE_BACKPOINTER(this));

		pAction->AddComputeOp(pDepthOp, RdrPass::LightCulling);
	}

	//////////////////////////////////////
	// Light culling
	if (tileCountChanged)
	{
		if (m_tiledLightData.hLightIndices)
			rResCommandList.ReleaseResource(m_tiledLightData.hLightIndices, CREATE_BACKPOINTER(this));
		m_tiledLightData.hLightIndices = RdrResourceSystem::CreateDataBuffer(nullptr
			, tileCountX * tileCountY * TILEDLIGHTING_BLOCK_SIZE
			, RdrResourceFormat::R16_UINT, RdrResourceAccessFlags::GpuRW
			, CREATE_BACKPOINTER(this));
	}

	// Update constants
	constantsSize = sizeof(TiledLightCullingParams);
	TiledLightCullingParams* pParams = (TiledLightCullingParams*)RdrFrameMem::AllocAligned(constantsSize, 16);

	Matrix44 mtxProj;
	rCamera.GetMatrices(pParams->mtxView, mtxProj);

	pParams->mtxView = Matrix44Transpose(pParams->mtxView);
	pParams->proj_11 = mtxProj._11;
	pParams->proj_22 = mtxProj._22;
	pParams->cameraNearDist = rCamera.GetNearDist();
	pParams->cameraFarDist = rCamera.GetFarDist();
	pParams->screenSize = Vec2(viewport.width, viewport.height);
	pParams->fovY = rCamera.GetFieldOfViewY();
	pParams->aspectRatio = rCamera.GetAspectRatio();
	pParams->spotLightCount = numSpotLights;
	pParams->pointLightCount = numPointLights;
	pParams->tileCountX = tileCountX;
	pParams->tileCountY = tileCountY;

	RdrConstantBufferHandle hCullConstants = RdrResourceSystem::CreateTempConstantBuffer(pParams, constantsSize, CREATE_BACKPOINTER(this));

	// Fill draw op
	{
		RdrComputeOp* pCullOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
		pCullOp->threads[0] = tileCountX;
		pCullOp->threads[1] = tileCountY;
		pCullOp->threads[2] = 1;

		RdrResourceHandle ahResources[] = { rLightParams.hSpotLightListRes, rLightParams.hPointLightListRes, m_tiledLightData.hDepthMinMaxTex };
		pCullOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));
		pCullOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_tiledLightData.hLightIndices, 1, CREATE_BACKPOINTER(this));
		pCullOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(&hCullConstants, 1, CREATE_BACKPOINTER(this));

		pAction->AddComputeOp(pCullOp, RdrPass::LightCulling);
	}
}

void RdrLighting::QueueVolumetricFog(RdrAction* pAction, const AssetLib::VolumetricFogSettings& rFogSettings, const RdrResourceHandle hLightIndicesRes)
{
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetResourceCommandList();
	const RdrActionLightResources& rLightParams = pAction->GetLightResources();
	const RdrGlobalConstants& rGlobalConstants = pAction->GetGlobalConstants();

	// Volumetric fog LUTs
	const Rect& viewport = pAction->GetViewport();
	UVec3 lutSize(
		RdrComputeOp::getThreadGroupCount((uint)viewport.width, VOLFOG_LUT_THREADS_X),
		RdrComputeOp::getThreadGroupCount((uint)viewport.height, VOLFOG_LUT_THREADS_Y),
		64);
	if (lutSize.x != m_fogLutSize.x || lutSize.y != m_fogLutSize.y)
	{
		if (m_hFogDensityLightLut)
			rResCommandList.ReleaseResource(m_hFogDensityLightLut, CREATE_BACKPOINTER(this));
		if (m_hFogFinalLut)
			rResCommandList.ReleaseResource(m_hFogFinalLut, CREATE_BACKPOINTER(this));

		m_hFogDensityLightLut = RdrResourceSystem::CreateTexture3D(
			lutSize.x, lutSize.y, lutSize.z,
			RdrResourceFormat::R16G16B16A16_FLOAT,
			RdrResourceAccessFlags::GpuRW,
			nullptr, CREATE_BACKPOINTER(this));

		m_hFogFinalLut = RdrResourceSystem::CreateTexture3D(
			lutSize.x, lutSize.y, lutSize.z,
			RdrResourceFormat::R16G16B16A16_FLOAT,
			RdrResourceAccessFlags::GpuRW,
			nullptr, CREATE_BACKPOINTER(this));

		m_fogLutSize = lutSize;
	}

	// Update constants
	VolumetricFogParams* pFogParams = (VolumetricFogParams*)RdrFrameMem::AllocAligned(sizeof(VolumetricFogParams), 16);
	pFogParams->lutSize = m_fogLutSize;
	pFogParams->farDepth = rFogSettings.farDepth;
	pFogParams->phaseG = rFogSettings.phaseG;
	pFogParams->absorptionCoeff = rFogSettings.absorptionCoeff;
	pFogParams->scatteringCoeff = rFogSettings.scatteringCoeff;

	m_hFogConstants = RdrResourceSystem::CreateUpdateConstantBuffer(m_hFogConstants,
		pFogParams, sizeof(VolumetricFogParams), RdrResourceAccessFlags::CpuRW_GpuRO, CREATE_BACKPOINTER(this));

	//////////////////////////////////////////////////////////////////////////
	// Participating media density and per-froxel lighting.
	{
		RdrComputeOp* pLightOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
		pLightOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::VolumetricFog_Light);

		pLightOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_hFogDensityLightLut, 1, CREATE_BACKPOINTER(this));

		RdrConstantBufferHandle ahConstantBuffers[] = { rGlobalConstants.hPsPerAction, rLightParams.hGlobalLightsCb, rGlobalConstants.hPsAtmosphere, m_hFogConstants };
		pLightOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(ahConstantBuffers, ARRAYSIZE(ahConstantBuffers), CREATE_BACKPOINTER(this));

		RdrSamplerState aSamplers[] = {
			RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false),
			RdrSamplerState(RdrComparisonFunc::LessEqual, RdrTexCoordMode::Clamp, false) };
		pLightOp->pSamplerDescriptorTable = RdrResourceSystem::CreateTempSamplerTable(aSamplers, ARRAYSIZE(aSamplers), CREATE_BACKPOINTER(this));

		RdrResourceHandle ahResources[] = { rLightParams.sharedResources.hSkyTransmittanceLut, m_hShadowMapTexArray, m_hShadowCubeMapTexArray, rLightParams.hSpotLightListRes, rLightParams.hPointLightListRes, hLightIndicesRes };
		pLightOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));

		pLightOp->threads[0] = RdrComputeOp::getThreadGroupCount(m_fogLutSize.x, VOLFOG_LUT_THREADS_X);
		pLightOp->threads[1] = RdrComputeOp::getThreadGroupCount(m_fogLutSize.y, VOLFOG_LUT_THREADS_Y);
		pLightOp->threads[2] = m_fogLutSize.z;
		pAction->AddComputeOp(pLightOp, RdrPass::VolumetricFog);
	}

	//////////////////////////////////////////////////////////////////////////
	// Scattering accumulation
	{
		RdrComputeOp* pAccumOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
		pAccumOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::VolumetricFog_Accum);

		RdrConstantBufferHandle ahConstantBuffers[] = { rGlobalConstants.hPsPerAction, m_hFogConstants };
		pAccumOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(ahConstantBuffers, ARRAYSIZE(ahConstantBuffers), CREATE_BACKPOINTER(this));

		RdrResourceHandle ahResources[] = { m_hFogDensityLightLut };
		pAccumOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));

		RdrResourceHandle ahWritableResources[] = { m_hFogFinalLut };
		pAccumOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(ahWritableResources, ARRAYSIZE(ahWritableResources), CREATE_BACKPOINTER(this));

		pAccumOp->threads[0] = RdrComputeOp::getThreadGroupCount(m_fogLutSize.x, VOLFOG_LUT_THREADS_X);
		pAccumOp->threads[1] = RdrComputeOp::getThreadGroupCount(m_fogLutSize.y, VOLFOG_LUT_THREADS_Y);
		pAccumOp->threads[2] = 1;
		pAction->AddComputeOp(pAccumOp, RdrPass::VolumetricFog);
	}
}
