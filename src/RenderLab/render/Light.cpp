#include "Precompiled.h"
#include "Light.h"
#include "RdrFrameMem.h"
#include "Renderer.h"
#include "Scene.h"
#include "Sky.h"
#include "RdrComputeOp.h"
#include "RdrOffscreenTasks.h"

namespace
{
	static uint s_environmentMapSize = 128;
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

LightList::LightList()
	: m_pScene(nullptr)
{

}

void LightList::Init(Scene* pScene)
{
	m_pScene = pScene;

	RdrResourceCommandList& rResCommands = g_pRenderer->GetPreFrameCommandList();
	m_hEnvironmentMapTexArray = rResCommands.CreateTextureCubeArray(s_environmentMapSize, s_environmentMapSize, MAX_ENVIRONMENT_MAPS, RdrResourceFormat::R16G16B16A16_FLOAT);
	m_hEnvironmentMapDepthBuffer = rResCommands.CreateTexture2D(s_environmentMapSize, s_environmentMapSize, RdrResourceFormat::D24_UNORM_S8_UINT, RdrResourceUsage::Default, nullptr);
	m_hEnvironmentMapDepthView = rResCommands.CreateDepthStencilView(m_hEnvironmentMapDepthBuffer);
	
	m_hShadowMapTexArray = rResCommands.CreateTexture2DArray(s_shadowMapSize, s_shadowMapSize, MAX_SHADOW_MAPS, RdrResourceFormat::D24_UNORM_S8_UINT);
	for (int i = 0; i < MAX_SHADOW_MAPS; ++i)
	{
		m_shadowMapDepthViews[i] = rResCommands.CreateDepthStencilView(m_hShadowMapTexArray, i, 1);
	}
	
	m_hShadowCubeMapTexArray = rResCommands.CreateTextureCubeArray(s_shadowCubeMapSize, s_shadowCubeMapSize, MAX_SHADOW_CUBEMAPS, RdrResourceFormat::D24_UNORM_S8_UINT);

#if USE_SINGLEPASS_SHADOW_CUBEMAP
	for (int i = 0; i < MAX_SHADOW_CUBEMAPS; ++i)
	{
		m_shadowCubeMapDepthViews[i] = rResCommands.CreateDepthStencilView(m_hShadowCubeMapTexArray, i * (int)CubemapFace::Count, (int)CubemapFace::Count);
	}
#else
	for (int i = 0; i < MAX_SHADOW_CUBEMAPS * CubemapFace::Count; ++i)
	{
		m_shadowCubeMapDepthViews[i] = rResCommands.CreateDepthStencilView(m_hShadowCubeMapTexArray, i, 1);
	}
#endif
}

void LightList::Cleanup()
{
	RdrResourceCommandList& rResCommands = g_pRenderer->GetPreFrameCommandList();
	if (m_hEnvironmentMapTexArray)
	{
		rResCommands.ReleaseResource(m_hEnvironmentMapTexArray);
		m_hEnvironmentMapTexArray = 0;
	}

	for (uint i = 0; i < MAX_SHADOW_MAPS; ++i)
	{
		if (m_shadowMapDepthViews[i])
		{
			rResCommands.ReleaseDepthStencilView(m_shadowMapDepthViews[i]);
			m_shadowMapDepthViews[i] = 0;
		}
	}

#if USE_SINGLEPASS_SHADOW_CUBEMAP
	uint numCubemapViews = MAX_SHADOW_CUBEMAPS;
#else
	uint numCubemapViews = MAX_SHADOW_CUBEMAPS * (uint)CubemapFace::Count;
#endif
	for (uint i = 0; i < numCubemapViews; ++i)
	{
		if (m_shadowCubeMapDepthViews[i])
		{
			rResCommands.ReleaseDepthStencilView(m_shadowCubeMapDepthViews[i]);
			m_shadowCubeMapDepthViews[i] = 0;
		}
	}

	if (m_hShadowMapTexArray)
	{
		rResCommands.ReleaseResource(m_hShadowMapTexArray);
		m_hShadowMapTexArray = 0;
	}

	if (m_hShadowCubeMapTexArray)
	{
		rResCommands.ReleaseResource(m_hShadowCubeMapTexArray);
		m_hShadowCubeMapTexArray = 0;
	}

	m_directionalLights.clear();
	m_environmentLights.clear();
	m_spotLights.clear();
	m_pointLights.clear();
}

void LightList::AddDirectionalLight(const Vec3& direction, const Vec3& color)
{
	DirectionalLight light;
	light.direction = direction;
	light.color = color;
	light.shadowMapIndex = -1;
	m_directionalLights.push(light);
}

void LightList::AddSpotLight(const Vec3& position, const Vec3& direction, const Vec3& color, float radius, float innerConeAngle, float outerConeAngle)
{
	SpotLight light;
	light.position = position;
	light.direction = direction;
	light.color = color;
	light.radius = radius;
	light.innerConeAngleCos = cosf(innerConeAngle);
	light.outerConeAngle = outerConeAngle;
	light.outerConeAngleCos = cosf(outerConeAngle);
	light.shadowMapIndex = -1;
	m_spotLights.push(light);
}

void LightList::AddPointLight(const Vec3& position, const Vec3& color, float radius)
{
	PointLight light;
	light.position = position;
	light.color = color;
	light.radius = radius;
	light.shadowMapIndex = -1;
	m_pointLights.push(light);
}

void LightList::AddEnvironmentLight(const Vec3& position)
{
	EnvironmentLight light;
	light.position = position;
	light.environmentMapIndex = m_environmentLights.size() + 1;
	m_environmentLights.push(light);

	Rect viewport(0.f, 0.f, (float)s_environmentMapSize, (float)s_environmentMapSize);
	RdrOffscreenTasks::QueueSpecularProbeCapture(position, m_pScene, viewport, m_hEnvironmentMapTexArray, light.environmentMapIndex, m_hEnvironmentMapDepthView);
}

void LightList::SetGlobalEnvironmentLight(const Vec3& position)
{
	m_globalEnvironmentLight.position = position;
	m_globalEnvironmentLight.environmentMapIndex = 0;

	Rect viewport(0.f, 0.f, (float)s_environmentMapSize, (float)s_environmentMapSize);
	RdrOffscreenTasks::QueueSpecularProbeCapture(position, m_pScene, viewport, m_hEnvironmentMapTexArray, m_globalEnvironmentLight.environmentMapIndex, m_hEnvironmentMapDepthView);
}

void LightList::InvalidateEnvironmentLights()
{
	Rect viewport(0.f, 0.f, (float)s_environmentMapSize, (float)s_environmentMapSize);

	// Global
	RdrOffscreenTasks::QueueSpecularProbeCapture(m_globalEnvironmentLight.position, m_pScene, viewport, m_hEnvironmentMapTexArray, m_globalEnvironmentLight.environmentMapIndex, m_hEnvironmentMapDepthView);

	// Local
	for (uint i = 0, count = m_environmentLights.size(); i < count; ++i)
	{
		RdrOffscreenTasks::QueueSpecularProbeCapture(m_environmentLights[i].position, m_pScene, viewport, 
			m_hEnvironmentMapTexArray, (m_environmentLights.size() - 1), m_hEnvironmentMapDepthView);
	}
}

void LightList::QueueDraw(Renderer& rRenderer, const Sky& rSky, const Camera& rCamera, const float sceneDepthMin, const float sceneDepthMax,
	RdrLightingMethod lightingMethod, RdrLightResources* pOutResources)
{
	Camera lightCamera;
	int curShadowMapIndex = 0;
	int curShadowCubeMapIndex = 0;
	int shadowMapsThisFrame = 0;

	// Update shadow casting lights and the global light constants
	// todo: Choose shadow lights based on location and dynamic object movement
	GlobalLightData* pGlobalLights = (GlobalLightData*)RdrFrameMem::AllocAligned(sizeof(GlobalLightData), 16);

	// Temporarily add sky light to the light list.
	m_directionalLights.push(rSky.GetSunLight());

	for (uint i = 0, count = m_directionalLights.size(); i < count; ++i)
	{
		DirectionalLight& rDirLight = m_directionalLights[i];
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
			float lamda = rSky.GetPssmLambda();

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

				rRenderer.QueueShadowMapPass(lightCamera, m_shadowMapDepthViews[curShadowMapIndex], viewport);

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

	for (uint i = 0, count = m_spotLights.size(); i < count; ++i)
	{
		SpotLight& rSpotLight = m_spotLights[i];
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
			rRenderer.QueueShadowMapPass(lightCamera, m_shadowMapDepthViews[curShadowMapIndex], viewport);

			rSpotLight.shadowMapIndex = curShadowMapIndex;
			++shadowMapsThisFrame;
			++curShadowMapIndex;
		}
		else
		{
			rSpotLight.shadowMapIndex = -1;
		}
	}

	for (uint i = 0, count = m_pointLights.size(); i < count; ++i)
	{
		PointLight& rPointLight = m_pointLights[i];
		int remainingShadowCubeMaps = MAX_SHADOW_CUBEMAPS - curShadowCubeMapIndex - 1;
		if (remainingShadowCubeMaps)
		{
			rPointLight.shadowMapIndex = curShadowCubeMapIndex + MAX_SHADOW_MAPS;

			Rect viewport(0.f, 0.f, (float)s_shadowCubeMapSize, (float)s_shadowCubeMapSize);
#if USE_SINGLEPASS_SHADOW_CUBEMAP
			rRenderer.QueueShadowCubeMapPass(rPointLight, m_shadowCubeMapDepthViews[curShadowCubeMapIndex], viewport);
#else
			for (uint face = 0; face < 6; ++face)
			{
				lightCamera.SetAsCubemapFace(light.position, (CubemapFace)face, 0.1f, light.radius * 2.f);
				rRenderer.QueueShadowMapPass(lightCamera, m_shadowCubeMapDepthViews[curShadowCubeMapIndex * 6 + face], viewport);
			}
#endif
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
	RdrResourceCommandList* pResCommandList = rRenderer.GetActionCommandList();

	// Update spot lights
	SpotLight* pSpotLights = (SpotLight*)RdrFrameMem::Alloc(sizeof(SpotLight) * m_spotLights.size());
	memcpy(pSpotLights, m_spotLights.getData(), sizeof(SpotLight) * m_spotLights.size());
	if (!m_hSpotLightListRes)
		m_hSpotLightListRes = pResCommandList->CreateStructuredBuffer(pSpotLights, m_spotLights.capacity(), sizeof(SpotLight), RdrResourceUsage::Dynamic);
	else
		pResCommandList->UpdateBuffer(m_hSpotLightListRes, pSpotLights, sizeof(SpotLight) * m_spotLights.size());

	// Update point lights
	PointLight* pPointLights = (PointLight*)RdrFrameMem::Alloc(sizeof(PointLight) * m_pointLights.size());
	memcpy(pPointLights, m_pointLights.getData(), sizeof(PointLight) * m_pointLights.size());
	if (!m_hPointLightListRes)
		m_hPointLightListRes = pResCommandList->CreateStructuredBuffer(pPointLights, m_pointLights.capacity(), sizeof(PointLight), RdrResourceUsage::Dynamic);
	else
		pResCommandList->UpdateBuffer(m_hPointLightListRes, pPointLights, sizeof(PointLight) * m_pointLights.size());

	// Update environment lights
	EnvironmentLight* pEnvironmentLights = (EnvironmentLight*)RdrFrameMem::Alloc(sizeof(EnvironmentLight) * m_environmentLights.size());
	memcpy(pEnvironmentLights, m_environmentLights.getData(), sizeof(EnvironmentLight) * m_environmentLights.size());
	if (!m_hEnvironmentLightListRes)
		m_hEnvironmentLightListRes = pResCommandList->CreateStructuredBuffer(pEnvironmentLights, m_environmentLights.capacity(), sizeof(EnvironmentLight), RdrResourceUsage::Dynamic);
	else
		pResCommandList->UpdateBuffer(m_hEnvironmentLightListRes, pEnvironmentLights, sizeof(EnvironmentLight) * m_environmentLights.size());

	// Light culling
	if (lightingMethod == RdrLightingMethod::Clustered)
	{
		QueueClusteredLightCulling(rRenderer, rCamera);
	}
	else
	{
		QueueTiledLightCulling(rRenderer, rCamera);
	}

	// Update global lights constant buffer last so the light culling data is up to date.
	pGlobalLights->numDirectionalLights = m_directionalLights.size();
	pGlobalLights->globalEnvironmentLight = m_globalEnvironmentLight;
	pGlobalLights->clusterTileSize = m_clusteredLightData.clusterTileSize;
	memcpy(pGlobalLights->directionalLights, m_directionalLights.getData(), sizeof(DirectionalLight) * pGlobalLights->numDirectionalLights);
	m_hGlobalLightsCb = pResCommandList->CreateUpdateConstantBuffer(m_hGlobalLightsCb,
		pGlobalLights, sizeof(GlobalLightData), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);

	// Remove sky light.
	m_directionalLights.pop();


	// Fill out resource params
	pOutResources->hGlobalLightsCb = m_hGlobalLightsCb;
	pOutResources->hSpotLightListRes = m_hSpotLightListRes;
	pOutResources->hPointLightListRes = m_hPointLightListRes;
	pOutResources->hEnvironmentLightListRes = m_hEnvironmentLightListRes;
	pOutResources->hShadowCubeMapTexArray = m_hShadowCubeMapTexArray;
	pOutResources->hShadowMapTexArray = m_hShadowMapTexArray;
	pOutResources->hEnvironmentMapTexArray = m_hEnvironmentMapTexArray;
	pOutResources->hLightIndicesRes = (lightingMethod == RdrLightingMethod::Clustered) ? 
		m_clusteredLightData.hLightIndices : m_tiledLightData.hLightIndices;
}

void LightList::QueueClusteredLightCulling(Renderer& rRenderer, const Camera& rCamera)
{
	RdrResourceCommandList* pResCommandList = rRenderer.GetActionCommandList();
	Vec2 viewportSize = rRenderer.GetViewportSize();

	// Determine the cluster tile size given two rules:
	//	1) The larger viewport dimension will have 16 tiles.
	//	2) Tile size must be a multiple of 8.
	const uint kMaxTilesPerDimension = 16;
	float maxDim = max(viewportSize.x, viewportSize.y);
	m_clusteredLightData.clusterTileSize = ((uint)ceil(maxDim / kMaxTilesPerDimension) + 7) & ~7;

	int clusterCountX = (int)ceil(viewportSize.x / m_clusteredLightData.clusterTileSize);
	int clusterCountY = (int)ceil(viewportSize.y / m_clusteredLightData.clusterTileSize);

	if (m_clusteredLightData.clusterCountX < clusterCountX || m_clusteredLightData.clusterCountY < clusterCountY)
	{
		// Resize buffers
		m_clusteredLightData.clusterCountX = max(16, clusterCountX);
		m_clusteredLightData.clusterCountY = max(16, clusterCountY);

		if (m_clusteredLightData.hLightIndices)
			pResCommandList->ReleaseResource(m_clusteredLightData.hLightIndices);
		m_clusteredLightData.hLightIndices = pResCommandList->CreateDataBuffer(nullptr, m_clusteredLightData.clusterCountX * m_clusteredLightData.clusterCountY * CLUSTEREDLIGHTING_DEPTH_SLICES * CLUSTEREDLIGHTING_BLOCK_SIZE, RdrResourceFormat::R16_UINT, RdrResourceUsage::Default);
	}

	RdrComputeOp* pCullOp = RdrFrameMem::AllocComputeOp();
	pCullOp->shader = RdrComputeShader::ClusteredLightCull;
	pCullOp->threads[0] = clusterCountX;
	pCullOp->threads[1] = clusterCountY;
	pCullOp->threads[2] = CLUSTEREDLIGHTING_DEPTH_SLICES;
	pCullOp->ahWritableResources.assign(0, m_clusteredLightData.hLightIndices);
	pCullOp->ahResources.assign(0, m_hSpotLightListRes);
	pCullOp->ahResources.assign(1, m_hPointLightListRes);

	uint constantsSize = sizeof(ClusteredLightCullingParams);
	ClusteredLightCullingParams* pParams = (ClusteredLightCullingParams*)RdrFrameMem::AllocAligned(constantsSize, 16);

	Matrix44 mtxProj;
	rCamera.GetMatrices(pParams->mtxView, mtxProj);

	pParams->mtxView = Matrix44Transpose(pParams->mtxView);
	pParams->proj_11 = mtxProj._11;
	pParams->proj_22 = mtxProj._22;
	pParams->screenSize = viewportSize;
	pParams->fovY = rCamera.GetFieldOfViewY();
	pParams->aspectRatio = rCamera.GetAspectRatio();
	pParams->clusterCountX = clusterCountX;
	pParams->clusterCountY = clusterCountY;
	pParams->clusterCountZ = CLUSTEREDLIGHTING_DEPTH_SLICES;
	pParams->clusterTileSize = m_clusteredLightData.clusterTileSize;
	pParams->spotLightCount = m_spotLights.size();
	pParams->pointLightCount = m_pointLights.size();

	m_clusteredLightData.hCullConstants = pResCommandList->CreateUpdateConstantBuffer(m_clusteredLightData.hCullConstants,
		pParams, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	pCullOp->ahConstantBuffers.assign(0, m_clusteredLightData.hCullConstants);

	rRenderer.AddComputeOpToPass(pCullOp, RdrPass::LightCulling);
}

void LightList::QueueTiledLightCulling(Renderer& rRenderer, const Camera& rCamera)
{
	RdrResourceCommandList* pResCommandList = rRenderer.GetActionCommandList();
	Vec2 viewportSize = rRenderer.GetViewportSize();

	uint tileCountX = RdrComputeOp::getThreadGroupCount((uint)viewportSize.x, TILEDLIGHTING_TILE_SIZE);
	uint tileCountY = RdrComputeOp::getThreadGroupCount((uint)viewportSize.y, TILEDLIGHTING_TILE_SIZE);
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
			pResCommandList->ReleaseResource(m_tiledLightData.hDepthMinMaxTex);
		m_tiledLightData.hDepthMinMaxTex = pResCommandList->CreateTexture2D(tileCountX, tileCountY, RdrResourceFormat::R16G16_FLOAT, RdrResourceUsage::Default, nullptr);
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

	m_tiledLightData.hDepthMinMaxConstants = pResCommandList->CreateUpdateConstantBuffer(m_tiledLightData.hDepthMinMaxConstants,
		pConstants, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);

	// Fill draw op
	RdrComputeOp* pDepthOp = RdrFrameMem::AllocComputeOp();
	pDepthOp->shader = RdrComputeShader::TiledDepthMinMax;
	pDepthOp->threads[0] = tileCountX;
	pDepthOp->threads[1] = tileCountY;
	pDepthOp->threads[2] = 1;
	pDepthOp->ahWritableResources.assign(0, m_tiledLightData.hDepthMinMaxTex);
	pDepthOp->ahResources.assign(0, rRenderer.GetPrimaryDepthBuffer());
	pDepthOp->ahConstantBuffers.assign(0, m_tiledLightData.hDepthMinMaxConstants);

	rRenderer.AddComputeOpToPass(pDepthOp, RdrPass::LightCulling);

	//////////////////////////////////////
	// Light culling
	if (tileCountChanged)
	{
		if (m_tiledLightData.hLightIndices)
			pResCommandList->ReleaseResource(m_tiledLightData.hLightIndices);
		m_tiledLightData.hLightIndices = pResCommandList->CreateDataBuffer(nullptr, tileCountX * tileCountY * TILEDLIGHTING_BLOCK_SIZE, RdrResourceFormat::R16_UINT, RdrResourceUsage::Default);
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
	pParams->screenSize = viewportSize;
	pParams->fovY = rCamera.GetFieldOfViewY();
	pParams->aspectRatio = rCamera.GetAspectRatio();
	pParams->spotLightCount = m_spotLights.size();
	pParams->pointLightCount = m_pointLights.size();
	pParams->tileCountX = tileCountX;
	pParams->tileCountY = tileCountY;

	m_tiledLightData.hCullConstants = pResCommandList->CreateUpdateConstantBuffer(m_tiledLightData.hCullConstants,
		pParams, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);

	// Fill draw op
	RdrComputeOp* pCullOp = RdrFrameMem::AllocComputeOp();
	pCullOp->threads[0] = tileCountX;
	pCullOp->threads[1] = tileCountY;
	pCullOp->threads[2] = 1;
	pCullOp->ahWritableResources.assign(0, m_tiledLightData.hLightIndices);
	pCullOp->ahResources.assign(0, m_hSpotLightListRes);
	pCullOp->ahResources.assign(1, m_hPointLightListRes);
	pCullOp->ahResources.assign(2, m_tiledLightData.hDepthMinMaxTex);
	pCullOp->ahConstantBuffers.assign(0, m_tiledLightData.hCullConstants);

	rRenderer.AddComputeOpToPass(pCullOp, RdrPass::LightCulling);
}
