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
	};


	Camera makeCameraForLight(const Light& light)
	{
		float angle = acosf(light.outerConeAngleCos) + Maths::DegToRad(5.f);
		Camera cam;

		switch (light.type)
		{
		case LightType::Directional:
			cam.SetAsOrtho(light.position, light.direction, 30, 30, -30.f, 30.f);
			break;
		case LightType::Spot:
		{
			float angle = acosf(light.outerConeAngleCos) + Maths::DegToRad(5.f);
			cam.SetAsPerspective(light.position, light.direction, angle * 2.f, 1.f, 0.1f, 1000.f);
			break;
		}
		case LightType::Point:
		{
			assert(false);
			break;
		}
		}
		return cam;
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

void LightList::PrepareDrawForScene(Renderer& rRenderer, const Camera& rCamera, const Scene& scene)
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

	int shadowLights[MAX_SHADOW_MAPS + MAX_SHADOW_CUBEMAPS];
	memset(shadowLights, -1, sizeof(shadowLights));

	bool changed = false;
	int curShadowMapIndex = 0;
	int curShadowCubeMapIndex = 0;
	// todo: Choose shadow lights based on location and dynamic object movement
	for (uint i = 0; i < m_lightCount; ++i)
	{
		Light& light = m_lights[i];
		if (!light.castsShadows)
			continue;

		bool hasMoreShadowMaps = (curShadowMapIndex < MAX_SHADOW_MAPS);
		bool hasMoreShadowCubeMaps = (curShadowCubeMapIndex < MAX_SHADOW_CUBEMAPS);
		if (light.type == LightType::Directional && hasMoreShadowMaps)
		{
			changed = true; // Directional lights always change.
			if (curShadowMapIndex != light.shadowMapIndex)
			{
				light.shadowMapIndex = curShadowMapIndex;
				changed = true;
			}

			shadowLights[light.shadowMapIndex] = i;

			Rect viewport(0.f, 0.f, (float)s_shadowMapSize, (float)s_shadowMapSize);
			rRenderer.BeginShadowMapAction(makeCameraForLight(light), m_shadowMapDepthViews[curShadowMapIndex], viewport);
			scene.QueueDraw(rRenderer);
			rRenderer.EndAction();

			++curShadowMapIndex;
		}
		else if (light.type == LightType::Spot && hasMoreShadowMaps)
		{
			if (curShadowMapIndex != light.shadowMapIndex)
			{
				light.shadowMapIndex = curShadowMapIndex;
				changed = true;
			}

			shadowLights[light.shadowMapIndex] = i;

			Rect viewport(0.f, 0.f, (float)s_shadowMapSize, (float)s_shadowMapSize);
			rRenderer.BeginShadowMapAction(makeCameraForLight(light), m_shadowMapDepthViews[curShadowMapIndex], viewport);
			scene.QueueDraw(rRenderer);
			rRenderer.EndAction();

			++curShadowMapIndex;
		}
		else if (light.type == LightType::Point && hasMoreShadowCubeMaps)
		{
			if (curShadowCubeMapIndex != (light.shadowMapIndex - MAX_SHADOW_MAPS))
			{
				light.shadowMapIndex = curShadowCubeMapIndex + MAX_SHADOW_MAPS;
				changed = true;
			}

			shadowLights[light.shadowMapIndex] = i;

			Rect viewport(0.f, 0.f, (float)s_shadowCubeMapSize, (float)s_shadowCubeMapSize);
#if USE_SINGLEPASS_SHADOW_CUBEMAP
			rRenderer.BeginShadowCubeMapAction(&light, m_shadowCubeMapDepthViews[curShadowCubeMapIndex], viewport);
			scene.QueueDraw(rRenderer);
			rRenderer.EndAction();
#else
			for (uint face = 0; face < 6; ++face)
			{
				Camera cam;
				cam.SetAsCubemapFace(light.position, (CubemapFace)face, 0.1f, light.radius * 2.f);
				rRenderer.BeginShadowMapAction(cam, m_shadowCubeMapDepthViews[curShadowCubeMapIndex * 6 + face], viewport);
				scene.QueueDraw(rRenderer);
				rRenderer.EndAction();
			}
#endif
			++curShadowCubeMapIndex;
		}

		if (!hasMoreShadowMaps && !hasMoreShadowCubeMaps)
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

		ShadowMapData* pShadowData = (ShadowMapData*)RdrScratchMem::Alloc(sizeof(ShadowMapData) * MAX_SHADOW_MAPS);
		for (int i = 0; i < curShadowMapIndex; ++i)
		{
			Light& light = m_lights[shadowLights[i]];
			Matrix44 mtxView;
			Matrix44 mtxProj;

			Camera cam = makeCameraForLight(light);
			cam.GetMatrices(mtxView, mtxProj);

			pShadowData[i].mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
			pShadowData[i].mtxViewProj = Matrix44Transpose(pShadowData[i].mtxViewProj);
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
