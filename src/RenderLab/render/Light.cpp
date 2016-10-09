#include "Precompiled.h"
#include "Light.h"
#include "RdrFrameMem.h"
#include "Renderer.h"
#include "Scene.h"
#include "Sky.h"

namespace
{
	const uint kSunLightIndex = 0;

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
	: m_lightCount(kSunLightIndex + 1)
	, m_prevLightCount(0)
{

}

void LightList::Cleanup()
{
	for (uint i = 0; i < MAX_SHADOW_MAPS; ++i)
	{
		if (m_shadowMapDepthViews[i])
		{
			RdrResourceSystem::ReleaseDepthStencilView(m_shadowMapDepthViews[i]);
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
			RdrResourceSystem::ReleaseDepthStencilView(m_shadowCubeMapDepthViews[i]);
			m_shadowCubeMapDepthViews[i] = 0;
		}
	}

	if (m_hSpotLightListRes)
	{
		RdrResourceSystem::ReleaseResource(m_hSpotLightListRes);
		m_hSpotLightListRes = 0;
	}

	if (m_hPointLightListRes)
	{
		RdrResourceSystem::ReleaseResource(m_hPointLightListRes);
		m_hPointLightListRes = 0;
	}

	if (m_hDirectionalLightListCb)
	{
		RdrResourceSystem::ReleaseConstantBuffer(m_hDirectionalLightListCb);
		m_hDirectionalLightListCb = 0;
	}

	if (m_hShadowMapDataRes)
	{
		RdrResourceSystem::ReleaseResource(m_hShadowMapDataRes);
		m_hShadowMapDataRes = 0;
	}

	if (m_hShadowMapTexArray)
	{
		RdrResourceSystem::ReleaseResource(m_hShadowMapTexArray);
		m_hShadowMapTexArray = 0;
	}

	if (m_hShadowCubeMapTexArray)
	{
		RdrResourceSystem::ReleaseResource(m_hShadowCubeMapTexArray);
		m_hShadowCubeMapTexArray = 0;
	}

	m_prevLightCount = 0;
	m_lightCount = 0;
}

void LightList::AddLight(const Light& light)
{
	assert(m_lightCount < ARRAY_SIZE(m_lights));

	m_lights[m_lightCount] = light;
	++m_lightCount;
}

void LightList::PrepareDraw(Renderer& rRenderer, const Sky& rSky, const Camera& rCamera, const float sceneDepthMin, const float sceneDepthMax)
{
	m_lights[kSunLightIndex] = rSky.GetSunLight();

	if (!m_hShadowMapTexArray)
	{
		m_hShadowMapTexArray = RdrResourceSystem::CreateTexture2DArray(s_shadowMapSize, s_shadowMapSize, MAX_SHADOW_MAPS, RdrResourceFormat::D24_UNORM_S8_UINT);
		for (int i = 0; i < MAX_SHADOW_MAPS; ++i)
		{
			m_shadowMapDepthViews[i] = RdrResourceSystem::CreateDepthStencilView(m_hShadowMapTexArray, i, 1);
		}
	}
	if (!m_hShadowCubeMapTexArray)
	{
		m_hShadowCubeMapTexArray = RdrResourceSystem::CreateTextureCubeArray(s_shadowCubeMapSize, s_shadowCubeMapSize, MAX_SHADOW_CUBEMAPS, RdrResourceFormat::D24_UNORM_S8_UINT);

#if USE_SINGLEPASS_SHADOW_CUBEMAP
		for (int i = 0; i < MAX_SHADOW_CUBEMAPS; ++i)
		{
			m_shadowCubeMapDepthViews[i] = RdrResourceSystem::CreateDepthStencilView(m_hShadowCubeMapTexArray, i * (int)CubemapFace::Count, (int)CubemapFace::Count);
		}
#else
		for (int i = 0; i < MAX_SHADOW_CUBEMAPS * CubemapFace::Count; ++i)
		{
			m_shadowCubeMapDepthViews[i] = RdrResourceSystem::CreateDepthStencilView(m_hShadowCubeMapTexArray, i, 1);
		}
#endif
	}

	Camera lightCamera;
	ShadowMapData* pShadowData = (ShadowMapData*)RdrFrameMem::Alloc(sizeof(ShadowMapData) * MAX_SHADOW_MAPS);
	bool changed = false;
	int curShadowMapIndex = 0;
	int curShadowCubeMapIndex = 0;
	int shadowMapsThisFrame = 0;

	// todo: Choose shadow lights based on location and dynamic object movement
	for (uint i = 0; i < m_lightCount; ++i)
	{
		Light& light = m_lights[i];
		if (!light.castsShadows)
			continue;

		int remainingShadowMaps = MAX_SHADOW_MAPS - curShadowMapIndex - 1;
		int remainingShadowCubeMaps = MAX_SHADOW_CUBEMAPS - curShadowCubeMapIndex - 1;
		if (light.type == LightType::Directional)
		{
			const int kNumCascades = 4;
			if (remainingShadowMaps >= kNumCascades)
			{
				// Directional lights always change because they're based on camera position/rotation
				changed = true;
				light.shadowMapIndex = curShadowMapIndex;

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
					Matrix44 lightViewMtx = getLightViewMatrix(center, light.direction);

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
					lightCamera.SetAsOrtho(center, light.direction, std::max(width, height), -500.f, 500.f);

					rRenderer.QueueShadowMapPass(lightCamera, m_shadowMapDepthViews[curShadowMapIndex], viewport);

					Matrix44 mtxView;
					Matrix44 mtxProj;
					lightCamera.GetMatrices(mtxView, mtxProj);
					pShadowData[curShadowMapIndex].mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
					pShadowData[curShadowMapIndex].mtxViewProj = Matrix44Transpose(pShadowData[curShadowMapIndex].mtxViewProj);
					pShadowData[curShadowMapIndex].partitionEndZ = (iPartition == kNumCascades - 1) ? FLT_MAX : farDepth;

					++shadowMapsThisFrame;
					++curShadowMapIndex;
					nearDepth = farDepth;
				}
			}
		}
		else if (light.type == LightType::Spot && remainingShadowMaps)
		{
			if (curShadowMapIndex != light.shadowMapIndex)
			{
				light.shadowMapIndex = curShadowMapIndex;
				changed = true;
			}

			Matrix44 mtxView;
			Matrix44 mtxProj;

			float angle = light.outerConeAngle + Maths::DegToRad(5.f);
			lightCamera.SetAsPerspective(light.position, light.direction, angle * 2.f, 1.f, 0.1f, 1000.f);
			lightCamera.GetMatrices(mtxView, mtxProj);

			pShadowData[curShadowMapIndex].mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
			pShadowData[curShadowMapIndex].mtxViewProj = Matrix44Transpose(pShadowData[curShadowMapIndex].mtxViewProj);

			Rect viewport(0.f, 0.f, (float)s_shadowMapSize, (float)s_shadowMapSize);
			rRenderer.QueueShadowMapPass(lightCamera, m_shadowMapDepthViews[curShadowMapIndex], viewport);

			++shadowMapsThisFrame;
			++curShadowMapIndex;
		}
		else if (light.type == LightType::Point && remainingShadowCubeMaps)
		{
			if (curShadowCubeMapIndex != (light.shadowMapIndex - MAX_SHADOW_MAPS))
			{
				light.shadowMapIndex = curShadowCubeMapIndex + MAX_SHADOW_MAPS;
				changed = true;
			}

			Rect viewport(0.f, 0.f, (float)s_shadowCubeMapSize, (float)s_shadowCubeMapSize);
#if USE_SINGLEPASS_SHADOW_CUBEMAP
			rRenderer.QueueShadowCubeMapPass(&light, m_shadowCubeMapDepthViews[curShadowCubeMapIndex], viewport);
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

		if ((shadowMapsThisFrame >= MAX_SHADOW_MAPS_PER_FRAME) || (!remainingShadowMaps && !remainingShadowCubeMaps))
			break;
	}

	changed = changed || (m_prevLightCount != m_lightCount);
	if (changed)
	{
		// todo: size/resize buffers appropriately
		DirectionalLightList* pDirectionalList = (DirectionalLightList*)RdrFrameMem::Alloc(sizeof(DirectionalLightList));
		SpotLight* pSpotLights = (SpotLight*)RdrFrameMem::Alloc(sizeof(SpotLight) * 2048);
		PointLight* pPointLights = (PointLight*)RdrFrameMem::Alloc(sizeof(PointLight) * 2048);

		uint numDirectionalLights = 0;
		m_numSpotLights = 0;
		m_numPointLights = 0;

		for (uint i = 0; i < m_lightCount; ++i)
		{
			Light& light = m_lights[i];
			switch (light.type)
			{
			case LightType::Directional:
			{
				DirectionalLight& rDirLight = pDirectionalList->lights[numDirectionalLights++];
				rDirLight.direction = light.direction;
				rDirLight.color = light.color;
				rDirLight.shadowMapIndex = light.shadowMapIndex;
			}
			break;
			case LightType::Point:
			{
				PointLight& rPointLight = pPointLights[m_numPointLights++];
				rPointLight.color = light.color;
				rPointLight.radius = light.radius;
				rPointLight.position = light.position;
				rPointLight.shadowMapIndex = light.shadowMapIndex;
			}
			break;
			case LightType::Spot:
			{
				SpotLight& rSpotLight = pSpotLights[m_numSpotLights++];
				rSpotLight.color = light.color;
				rSpotLight.direction = light.direction;
				rSpotLight.radius = light.radius;
				rSpotLight.innerConeAngleCos = cosf(light.innerConeAngle);
				rSpotLight.outerConeAngleCos = cosf(light.outerConeAngle);
				rSpotLight.outerConeAngle = light.outerConeAngle;
				rSpotLight.position = light.position;
				rSpotLight.shadowMapIndex = light.shadowMapIndex;
			}
			break;
			}
		}

		if (!m_hSpotLightListRes)
		{
			m_hSpotLightListRes = RdrResourceSystem::CreateStructuredBuffer(pSpotLights, 2048, sizeof(SpotLight), RdrResourceUsage::Dynamic);
		}
		else
		{
			RdrResourceSystem::UpdateBuffer(m_hSpotLightListRes, pSpotLights, m_numSpotLights);
		}

		if (!m_hPointLightListRes)
		{
			m_hPointLightListRes = RdrResourceSystem::CreateStructuredBuffer(pPointLights, 2048, sizeof(PointLight), RdrResourceUsage::Dynamic);
		}
		else
		{
			RdrResourceSystem::UpdateBuffer(m_hPointLightListRes, pPointLights, m_numPointLights);
		}

		pDirectionalList->numLights = numDirectionalLights;
		if (!m_hDirectionalLightListCb)
		{
			m_hDirectionalLightListCb = RdrResourceSystem::CreateConstantBuffer(pDirectionalList, sizeof(DirectionalLightList), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
		}
		else
		{
			RdrResourceSystem::UpdateConstantBuffer(m_hDirectionalLightListCb, pDirectionalList);
		}

		if (m_hShadowMapDataRes)
		{
			RdrResourceSystem::UpdateBuffer(m_hShadowMapDataRes, pShadowData);
		}
		else
		{
			m_hShadowMapDataRes = RdrResourceSystem::CreateStructuredBuffer(pShadowData, MAX_SHADOW_MAPS, sizeof(ShadowMapData), RdrResourceUsage::Dynamic);
		}

		m_prevLightCount = m_lightCount;
	}
}
