#include "Precompiled.h"
#include "Light.h"
#include "Renderer.h"

#define MAX_SHADOWMAPS_PER_FRAME 10
static uint s_shadowMapSize = 2048;

struct ShadowMapData
{
	Matrix44 mtxViewProj;
};

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

void LightList::PrepareDraw(Renderer& rRenderer)
{
	RdrContext* pContext = rRenderer.GetContext();
	if (!m_hShadowMapTexArray)
	{
		m_hShadowMapTexArray = pContext->CreateTexture2DArray(s_shadowMapSize, s_shadowMapSize, MAX_SHADOWMAPS_PER_FRAME, kResourceFormat_D16);
	}

	int shadowLights[MAX_SHADOWMAPS_PER_FRAME];
	memset(shadowLights, -1, sizeof(shadowLights));

	int curShadowMapIndex = 0;
	// todo: Choose shadow lights
	for (uint i = 0; i < m_lightCount; ++i)
	{
		Light& light = m_lights[i];
		if (!light.castsShadows)
			continue;

		if (light.type == kLightType_Directional)
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
		// todo: update instead of re-create
		if (m_hLightListRes)
			pContext->ReleaseTexture(m_hLightListRes);
		m_hLightListRes = pContext->CreateStructuredBuffer(m_lights, m_lightCount, sizeof(Light));

		if (m_hShadowMapDataRes)
			pContext->ReleaseTexture(m_hShadowMapDataRes);

		ShadowMapData shadowData[MAX_SHADOWMAPS_PER_FRAME];
		for (int i = 0; i < curShadowMapIndex; ++i)
		{
			Matrix44 view_mat = Matrix44LookToLH(Vec3::kOrigin, m_lights[shadowLights[i]].direction, Vec3::kUnitY);
			Matrix44 proj_mat = DirectX::XMMatrixOrthographicLH(30, 30, -30, 30.f);

			shadowData[i].mtxViewProj = Matrix44Multiply(view_mat, proj_mat);
			shadowData[i].mtxViewProj = Matrix44Transpose(shadowData[i].mtxViewProj);
		}
		m_hShadowMapDataRes = pContext->CreateStructuredBuffer(shadowData, MAX_SHADOWMAPS_PER_FRAME, sizeof(ShadowMapData));
	}
}
