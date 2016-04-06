#include "Precompiled.h"
#include "Light.h"
#include "RdrTransientHeap.h"
#include "Renderer.h"
#include "Scene.h"

static uint s_shadowMapSize = 2048;
static uint s_shadowCubeMapSize = 512;

struct ShadowMapData
{
	Matrix44 mtxViewProj;
};

Camera Light::MakeCamera(CubemapFace face) const
{
	float angle = acosf(outerConeAngleCos) + Maths::DegToRad(5.f);
	Camera cam;

	switch (type)
	{
	case kLightType_Directional:
		cam.SetAsOrtho(position, direction, 30, 30, -30.f, 30.f);
		break;
	case kLightType_Spot:
	{
		float angle = acosf(outerConeAngleCos) + Maths::DegToRad(5.f);
		cam.SetAsPerspective(position, direction, angle * 2.f, 1.f, 0.1f, 1000.f);
		break;
	}
	case kLightType_Point:
	{
		static const Vec3 faceDirs[6] = {
			Vec3::kUnitX,
			-Vec3::kUnitX,
			Vec3::kUnitY,
			-Vec3::kUnitY,
			Vec3::kUnitZ,
			-Vec3::kUnitZ
		};
		float angle = Maths::DegToRad(90.f);
		cam.SetAsPerspective(position, faceDirs[face], angle, 1.f, 0.1f, radius * 2.f);
		break;
	}
	}
	return cam;
}


LightList::LightList()
	: m_lightCount(0)
	, m_prevLightCount(0)
{

}

void LightList::AddLight(Light& light)
{
	assert(m_lightCount < ARRAYSIZE(m_lights));

	m_lights[m_lightCount] = light;
	++m_lightCount;
}

void LightList::PrepareDrawForScene(Renderer& rRenderer, const Camera& rCamera, const Scene& scene)
{
	static const RdrResourceFormat kDepthFormat = kResourceFormat_D16;
	RdrResourceSystem& resourceSystem = rRenderer.GetResourceSystem();

	if (!m_hShadowMapTexArray)
	{
		m_hShadowMapTexArray = resourceSystem.CreateTexture2DArray(s_shadowMapSize, s_shadowMapSize, MAX_SHADOW_MAPS, kDepthFormat);
		for (int i = 0; i < MAX_SHADOW_MAPS; ++i)
		{
			m_shadowMapDepthViews[i] = resourceSystem.CreateDepthStencilView(m_hShadowMapTexArray, i);
		}
	}
	if (!m_hShadowCubeMapTexArray)
	{
		m_hShadowCubeMapTexArray = resourceSystem.CreateTextureCubeArray(s_shadowCubeMapSize, s_shadowCubeMapSize, MAX_SHADOW_CUBEMAPS, kDepthFormat);

#if USE_SINGLEPASS_SHADOW_CUBEMAP
		for (int i = 0; i < MAX_SHADOW_CUBEMAPS; ++i)
		{
			m_shadowCubeMapViews[i] = rRenderer.CreateRenderTargetView(m_hShadowCubeMapTexArray, i, kResourceFormat_R16_UNORM);
		}
#else
		for (int i = 0; i < MAX_SHADOW_CUBEMAPS * 6; ++i)
		{
			m_shadowCubeMapDepthViews[i] = resourceSystem.CreateDepthStencilView(m_hShadowCubeMapTexArray, i);
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
		if (light.type == kLightType_Directional && hasMoreShadowMaps)
		{
			changed = true; // Directional lights always change.
			if (curShadowMapIndex != light.shadowMapIndex)
			{
				light.shadowMapIndex = curShadowMapIndex;
				changed = true;
			}

			shadowLights[light.shadowMapIndex] = i;

			Rect viewport(0.f, 0.f, (float)s_shadowMapSize, (float)s_shadowMapSize);
			rRenderer.BeginShadowMapAction(light.MakeCamera(), m_shadowMapDepthViews[curShadowMapIndex], viewport);
			scene.QueueDraw(rRenderer);
			rRenderer.EndAction();

			++curShadowMapIndex;
		}
		else if (light.type == kLightType_Spot && hasMoreShadowMaps)
		{
			if (curShadowMapIndex != light.shadowMapIndex)
			{
				light.shadowMapIndex = curShadowMapIndex;
				changed = true;
			}

			shadowLights[light.shadowMapIndex] = i;

			Rect viewport(0.f, 0.f, (float)s_shadowMapSize, (float)s_shadowMapSize);
			rRenderer.BeginShadowMapAction(light.MakeCamera(), m_shadowMapDepthViews[curShadowMapIndex], viewport);
			scene.QueueDraw(rRenderer);
			rRenderer.EndAction();

			++curShadowMapIndex;
		}
		else if (light.type == kLightType_Point && hasMoreShadowCubeMaps)
		{
			if (curShadowCubeMapIndex != (light.shadowMapIndex - MAX_SHADOW_MAPS))
			{
				light.shadowMapIndex = curShadowCubeMapIndex + MAX_SHADOW_MAPS;
				changed = true;
			}

			shadowLights[light.shadowMapIndex] = i;

			Rect viewport(0.f, 0.f, (float)s_shadowCubeMapSize, (float)s_shadowCubeMapSize);
#if USE_SINGLEPASS_SHADOW_CUBEMAP
			rRenderer.BeginShadowCubeMapAction(&light, &m_shadowCubeMapViews[curShadowCubeMapIndex * 6], viewport);
			scene.QueueDraw(rRenderer);
			rRenderer.EndAction();
#else
			for (uint face = 0; face < 6; ++face)
			{
				rRenderer.BeginShadowMapAction(light.MakeCamera((CubemapFace)face), m_shadowCubeMapDepthViews[curShadowCubeMapIndex * 6 + face], viewport);
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
				rRenderer.GetResourceSystem().ReleaseResource(m_hLightListRes);
			m_hLightListRes = rRenderer.GetResourceSystem().CreateStructuredBuffer(m_lights, m_lightCount, sizeof(Light), false, kRdrResourceUsage_Dynamic);
		}
		else
		{
			rRenderer.GetResourceSystem().UpdateStructuredBuffer(m_hLightListRes, m_lights, false);
		}

		//ShadowMapData* pShadowData = (ShadowMapData*)RdrTransientHeap::Alloc(sizeof(ShadowMapData) * MAX_SHADOW_MAPS + MAX_SHADOW_CUBEMAPS);
		static ShadowMapData pShadowData[MAX_SHADOW_MAPS + MAX_SHADOW_CUBEMAPS];
		for (int i = 0; i < curShadowMapIndex; ++i)
		{
			Light& light = m_lights[shadowLights[i]];
			Matrix44 mtxView;
			Matrix44 mtxProj;

			Camera cam = light.MakeCamera();
			cam.GetMatrices(mtxView, mtxProj);

			pShadowData[i].mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
			pShadowData[i].mtxViewProj = Matrix44Transpose(pShadowData[i].mtxViewProj);
		}

		for (int i = 0; i < curShadowCubeMapIndex; ++i)
		{
			uint shadowIndex = MAX_SHADOW_MAPS + i;
			Light& light = m_lights[shadowLights[shadowIndex]];
			Matrix44 mtxView;
			Matrix44 mtxProj;

			Camera cam = light.MakeCamera();
			cam.GetMatrices(mtxView, mtxProj);
			pShadowData[shadowIndex].mtxViewProj = Matrix44Transpose(mtxProj);
		}

		if (m_hShadowMapDataRes)
		{
			resourceSystem.UpdateStructuredBuffer(m_hShadowMapDataRes, pShadowData, false);
		}
		else
		{
			m_hShadowMapDataRes = resourceSystem.CreateStructuredBuffer(pShadowData, MAX_SHADOW_MAPS + MAX_SHADOW_CUBEMAPS, sizeof(ShadowMapData), false, kRdrResourceUsage_Dynamic);
		}

		m_prevLightCount = m_lightCount;
	}
}
