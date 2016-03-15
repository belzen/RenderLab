#include "Precompiled.h"
#include "Light.h"
#include "Renderer.h"

#define MAX_SHADOWMAPS_PER_FRAME 10
static uint s_shadowMapSize = 2048;

struct ShadowMapData
{
	Matrix44 mtxViewProj;
};

Matrix44 Light::GetViewMatrix(const Camera& rCamera, int face) const
{
	Matrix44 mtxView;

	Vec3 upDir = Vec3::kUnitY;
	if (direction.y > 0.99f)
		upDir = -Vec3::kUnitZ;
	else if (direction.y < 0.99f)
		upDir = Vec3::kUnitZ;

	switch (type)
	{
	case kLightType_Directional:
		mtxView = Matrix44LookToLH(rCamera.GetPosition(), direction, upDir);
		break;
	case kLightType_Spot:
		mtxView = Matrix44LookToLH(position, direction, upDir);
		break;
	}
	return mtxView;
}

Matrix44 Light::GetProjMatrix(const Camera& rCamera) const
{
	Matrix44 mtxProj;
	switch (type)
	{
	case kLightType_Directional:
		// todo: ortho size relative to frustum
		mtxProj = DirectX::XMMatrixOrthographicLH(30, 30, -30, 30.f);
		break;
	case kLightType_Spot:
	{
		float angle = acosf(outerConeAngleCos) + Maths::DegToRad(5.f);
		mtxProj = Matrix44PerspectiveFovLH(angle * 2.f, 1.f, 0.01f, 1000.f);
		break;
	}
	case kLightType_Point:
	{
		float angle = Maths::DegToRad(90.f);
		mtxProj = Matrix44PerspectiveFovLH(angle, 1.f, 0.01f, 1000.f);
		break;
	}
	}
	return mtxProj;
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

void LightList::PrepareDraw(Renderer& rRenderer, const Camera& rCamera)
{
	RdrContext* pContext = rRenderer.GetContext();
	if (!m_hShadowMapTexArray)
	{
		m_hShadowMapTexArray = pContext->CreateTexture2DArray(s_shadowMapSize, s_shadowMapSize, MAX_SHADOWMAPS_PER_FRAME, kResourceFormat_D16);
	}

	int shadowLights[MAX_SHADOWMAPS_PER_FRAME];
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
			rRenderer.QueueShadowMap(&light, m_hShadowMapTexArray, curShadowMapIndex, viewport);
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
			rRenderer.QueueShadowMap(&light, m_hShadowMapTexArray, curShadowMapIndex, viewport);
			++curShadowMapIndex;
		}
	}

	if (m_changed)
	{
		if (m_hLightListRes)
			pContext->ReleaseResource(m_hLightListRes);
		m_hLightListRes = pContext->CreateStructuredBuffer(m_lights, m_lightCount, sizeof(Light));

		ShadowMapData shadowData[MAX_SHADOWMAPS_PER_FRAME];
		for (int i = 0; i < curShadowMapIndex; ++i)
		{
			Light& light = m_lights[shadowLights[i]];
			Matrix44 mtxView;
			Matrix44 mtxProj;

			mtxView = light.GetViewMatrix(rCamera, 0);
			mtxProj = light.GetProjMatrix(rCamera);

			shadowData[i].mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
			shadowData[i].mtxViewProj = Matrix44Transpose(shadowData[i].mtxViewProj);
		}

		if (m_hShadowMapDataRes)
			pContext->ReleaseResource(m_hShadowMapDataRes);
		m_hShadowMapDataRes = pContext->CreateStructuredBuffer(shadowData, MAX_SHADOWMAPS_PER_FRAME, sizeof(ShadowMapData));

		m_changed = false;
	}
}
