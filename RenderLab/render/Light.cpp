#include "Precompiled.h"
#include "Light.h"
#include "Renderer.h"
#include "Scene.h"

static uint s_shadowMapSize = 2048;

struct ShadowMapData
{
	Matrix44 mtxViewProj;
};

Camera Light::MakeCamera(uint face) const
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
		float angle = Maths::DegToRad(90.f);
		cam.SetAsPerspective(position, direction, angle * 2.f, 1.f, 0.1f, 1000.f);
		break;
	}
	}
	return cam;
}


LightList::LightList()
	: m_lightCount(0)
{

}

void LightList::AddLight(Light& light)
{
	assert(m_lightCount < ARRAYSIZE(m_lights));

	m_lights[m_lightCount] = light;
	++m_lightCount;
	m_changed = true;
}

void LightList::PrepareDrawForScene(Renderer& rRenderer, const Camera& rCamera, const Scene& scene)
{
	RdrContext* pContext = rRenderer.GetContext();
	if (!m_hShadowMapTexArray)
	{
		m_hShadowMapTexArray = pContext->CreateTexture2DArray(s_shadowMapSize, s_shadowMapSize, MAX_SHADOWMAPS, kResourceFormat_D16);
		for (int i = 0; i < MAX_SHADOWMAPS; ++i)
		{
			m_shadowMapDepthViews[i] = pContext->CreateDepthStencilView(m_hShadowMapTexArray, i);
		}
	}

	int shadowLights[MAX_SHADOWMAPS];
	memset(shadowLights, -1, sizeof(shadowLights));

	int curShadowMapIndex = 0;
	// todo: Choose shadow lights based on location and dynamic object movement
	for (uint i = 0; i < m_lightCount; ++i)
	{
		Light& light = m_lights[i];
		if (!light.castsShadows)
			continue;

		if (light.type == kLightType_Directional)
		{
			m_changed = true; // Directional lights always change.
			if (curShadowMapIndex != light.shadowMapIndex)
			{
				light.shadowMapIndex = curShadowMapIndex;
				m_changed = true;
			}

			shadowLights[curShadowMapIndex] = i;

			Rect viewport(0.f, 0.f, (float)s_shadowMapSize, (float)s_shadowMapSize);
			rRenderer.BeginShadowMapAction(&light, m_shadowMapDepthViews[curShadowMapIndex], viewport);
			scene.QueueDraw(rRenderer);
			rRenderer.EndAction();

			++curShadowMapIndex;
		}
		else if (light.type == kLightType_Spot)
		{
			if (curShadowMapIndex != light.shadowMapIndex)
			{
				light.shadowMapIndex = curShadowMapIndex;
				m_changed = true;
			}

			shadowLights[curShadowMapIndex] = i;

			Rect viewport(0.f, 0.f, (float)s_shadowMapSize, (float)s_shadowMapSize);
			rRenderer.BeginShadowMapAction(&light, m_shadowMapDepthViews[curShadowMapIndex], viewport);
			scene.QueueDraw(rRenderer);
			rRenderer.EndAction();

			++curShadowMapIndex;
		}
	}

	if (m_changed)
	{
		if (m_hLightListRes)
			pContext->ReleaseResource(m_hLightListRes);
		m_hLightListRes = pContext->CreateStructuredBuffer(m_lights, m_lightCount, sizeof(Light));

		ShadowMapData shadowData[MAX_SHADOWMAPS];
		for (int i = 0; i < curShadowMapIndex; ++i)
		{
			Light& light = m_lights[shadowLights[i]];
			Matrix44 mtxView;
			Matrix44 mtxProj;

			Camera cam = light.MakeCamera(0);
			cam.GetMatrices(mtxView, mtxProj);

			shadowData[i].mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
			shadowData[i].mtxViewProj = Matrix44Transpose(shadowData[i].mtxViewProj);
		}

		if (m_hShadowMapDataRes)
			pContext->ReleaseResource(m_hShadowMapDataRes);
		m_hShadowMapDataRes = pContext->CreateStructuredBuffer(shadowData, MAX_SHADOWMAPS, sizeof(ShadowMapData));

		m_changed = false;
	}
}
