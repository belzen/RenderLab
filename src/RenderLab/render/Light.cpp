#include "Precompiled.h"
#include "Light.h"
#include "RdrScratchMem.h"
#include "Renderer.h"
#include "Scene.h"

namespace
{
	static uint s_shadowMapSize = 1024;
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
	: m_lightCount(0)
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

	if (m_hLightListRes)
	{
		RdrResourceSystem::ReleaseResource(m_hLightListRes);
		m_hLightListRes = 0;
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

void LightList::AddLight(Light& light)
{
	assert(m_lightCount < ARRAY_SIZE(m_lights));

	m_lights[m_lightCount] = light;
	++m_lightCount;
}

void LightList::PrepareDrawForScene(Renderer& rRenderer, const Scene& scene)
{
	if (!m_hShadowMapTexArray)
	{
		m_hShadowMapTexArray = RdrResourceSystem::CreateTexture2DArray(s_shadowMapSize, s_shadowMapSize, MAX_SHADOW_MAPS, RdrResourceFormat::D16);
		for (int i = 0; i < MAX_SHADOW_MAPS; ++i)
		{
			m_shadowMapDepthViews[i] = RdrResourceSystem::CreateDepthStencilView(m_hShadowMapTexArray, i, 1);
		}
	}
	if (!m_hShadowCubeMapTexArray)
	{
		m_hShadowCubeMapTexArray = RdrResourceSystem::CreateTextureCubeArray(s_shadowCubeMapSize, s_shadowCubeMapSize, MAX_SHADOW_CUBEMAPS, RdrResourceFormat::D16);

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
	ShadowMapData* pShadowData = (ShadowMapData*)RdrScratchMem::Alloc(sizeof(ShadowMapData) * MAX_SHADOW_MAPS);
	bool changed = false;
	int curShadowMapIndex = 0;
	int curShadowCubeMapIndex = 0;

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
				const Camera& rSceneCamera = scene.GetMainCamera();
				float zMin = rSceneCamera.GetNearDist();
				float zMax = rSceneCamera.GetFarDist();
				float zDiff = (zMax - zMin);

				float nearDepth = zMin;
				float lamda = 0.9f;

				for (int iPartition = 0; iPartition < kNumCascades; ++iPartition)
				{
					float zUni = zMin + (zDiff / kNumCascades) * (iPartition + 1);
					float zLog = zMin * powf(zMax / zMin, (iPartition + 1) / (float)kNumCascades);
					float farDepth = lamda * zLog + (1.f - lamda) * zUni;

					Vec3 center = rSceneCamera.GetPosition() + rSceneCamera.GetDirection() * ((farDepth + nearDepth) * 0.5f);
					Matrix44 lightViewMtx = getLightViewMatrix(center, light.direction);

					Quad nearQuad = rSceneCamera.GetFrustumQuad(nearDepth);
					QuadTransform(nearQuad, lightViewMtx);

					Quad farQuad = rSceneCamera.GetFrustumQuad(farDepth);
					QuadTransform(farQuad, lightViewMtx);

					Vec3 boundsMin(FLT_MAX, FLT_MAX, FLT_MAX);
					Vec3 boundsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
					accumQuadMinMax(nearQuad, boundsMin, boundsMax);
					accumQuadMinMax(farQuad, boundsMin, boundsMax);

					float height = (boundsMax.y - boundsMin.y);
					float width = (boundsMax.x - boundsMin.x);
					lightCamera.SetAsOrtho(center, light.direction, std::max(width, height), -500.f, 500.f);

					rRenderer.BeginShadowMapAction(lightCamera, scene, m_shadowMapDepthViews[curShadowMapIndex], viewport);
					rRenderer.EndAction();

					Matrix44 mtxView;
					Matrix44 mtxProj;
					lightCamera.GetMatrices(mtxView, mtxProj);
					pShadowData[curShadowMapIndex].mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
					pShadowData[curShadowMapIndex].mtxViewProj = Matrix44Transpose(pShadowData[curShadowMapIndex].mtxViewProj);
					pShadowData[curShadowMapIndex].partitionEndZ = (iPartition == kNumCascades - 1) ? FLT_MAX : farDepth;

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

			float angle = acosf(light.outerConeAngleCos) + Maths::DegToRad(5.f);
			lightCamera.SetAsPerspective(light.position, light.direction, angle * 2.f, 1.f, 0.1f, 1000.f);
			lightCamera.GetMatrices(mtxView, mtxProj);

			pShadowData[curShadowMapIndex].mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
			pShadowData[curShadowMapIndex].mtxViewProj = Matrix44Transpose(pShadowData[curShadowMapIndex].mtxViewProj);

			Rect viewport(0.f, 0.f, (float)s_shadowMapSize, (float)s_shadowMapSize);
			rRenderer.BeginShadowMapAction(lightCamera, scene, m_shadowMapDepthViews[curShadowMapIndex], viewport);
			rRenderer.EndAction();

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
			rRenderer.BeginShadowCubeMapAction(&light, scene, m_shadowCubeMapDepthViews[curShadowCubeMapIndex], viewport);
			rRenderer.EndAction();
#else
			for (uint face = 0; face < 6; ++face)
			{
				lightCamera.SetAsCubemapFace(light.position, (CubemapFace)face, 0.1f, light.radius * 2.f);
				rRenderer.BeginShadowMapAction(lightCamera, m_shadowCubeMapDepthViews[curShadowCubeMapIndex * 6 + face], viewport);
				scene.QueueDraw(rRenderer);
				rRenderer.EndAction();
			}
#endif
			++curShadowCubeMapIndex;
		}

		if (!remainingShadowMaps && !remainingShadowCubeMaps)
			break;
	}

	changed = changed || (m_prevLightCount != m_lightCount);
	if (changed)
	{
		if (m_prevLightCount != m_lightCount)
		{
			// todo: m_lights isn't threadsafe when queueing a create
			if (m_hLightListRes)
				RdrResourceSystem::ReleaseResource(m_hLightListRes);
			m_hLightListRes = RdrResourceSystem::CreateStructuredBuffer(m_lights, m_lightCount, sizeof(Light), RdrResourceUsage::Dynamic);
		}
		else
		{
			RdrResourceSystem::UpdateStructuredBuffer(m_hLightListRes, m_lights);
		}

		if (m_hShadowMapDataRes)
		{
			RdrResourceSystem::UpdateStructuredBuffer(m_hShadowMapDataRes, pShadowData);
		}
		else
		{
			m_hShadowMapDataRes = RdrResourceSystem::CreateStructuredBuffer(pShadowData, MAX_SHADOW_MAPS, sizeof(ShadowMapData), RdrResourceUsage::Dynamic);
		}

		m_prevLightCount = m_lightCount;
	}
}
