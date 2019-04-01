#pragma once

#include "AssetLib/AssetLibForwardDecl.h"
#include "Math.h"
#include "RdrShaderTypes.h"
#include "RdrShaders.h"
#include "RdrResource.h"
#include "UtilsLib/FixedVector.h"
#include "../../data/shaders/light_types.h"
#include "Camera.h"

class RdrAction;
class Camera;
class Light;

#define MAX_SHADOW_MAPS_PER_FRAME 10
#define USE_SINGLEPASS_SHADOW_CUBEMAP 1 //donotcheckin

enum class RdrLightingMethod
{
	Tiled,
	Clustered
};

struct RdrTiledLightingData
{
	RdrResourceHandle       hLightIndices;
	RdrResourceHandle	    hDepthMinMaxTex;
	int					    tileCountX;
	int					    tileCountY;
};

struct RdrClusteredLightingData
{
	RdrResourceHandle       hLightIndices;
	int					    clusterCountX;
	int					    clusterCountY;
	int					    clusterCountZ;
	int						clusterTileSize;
};

struct RdrSharedLightResources
{
	RdrResourceHandle hLightIndicesRes;

	RdrResourceHandle hSkyTransmittanceLut;
	RdrResourceHandle hVolumetricFogLut;
	RdrResourceHandle hEnvironmentMapTexArray;
	RdrResourceHandle hShadowMapTexArray;
	RdrResourceHandle hShadowCubeMapTexArray;
};

struct RdrActionLightResources
{
	RdrConstantBufferHandle hGlobalLightsCb;
	RdrResourceHandle hSpotLightListRes;
	RdrResourceHandle hPointLightListRes;
	RdrResourceHandle hEnvironmentLightListRes;

	RdrConstantBufferHandle hDepthMinMaxConstants;
	RdrConstantBufferHandle hCullConstants;

	// Not owned by action.  Filled in each frame by RdrLighting::QueueDraw()
	RdrSharedLightResources sharedResources;
};

struct RdrLightList
{
	void AddLight(const Light* pLight);
	void Clear();

	FixedVector<DirectionalLight, 4> m_directionalLights;
	FixedVector<PointLight, 256> m_pointLights;
	FixedVector<SpotLight, 256> m_spotLights;
	FixedVector<EnvironmentLight, 64> m_environmentLights;
	EnvironmentLight m_globalEnvironmentLight;
};

class RdrLighting
{
public:
	RdrLighting();

	void Cleanup();

	void QueueDraw(RdrAction* pAction, RdrLightList* pLights, RdrLightingMethod lightingMethod,
		const AssetLib::VolumetricFogSettings& rFog, const float sceneDepthMin, const float sceneDepthMax,
		RdrActionLightResources* pOutResources);

private:
	void LazyInitShadowResources();

	void QueueClusteredLightCulling(RdrAction* pAction, const Camera& rCamera, int numSpotLights, int numPointLights,
		RdrActionLightResources* pOutResources);
	void QueueTiledLightCulling(RdrAction* pAction, const Camera& rCamera, int numSpotLights, int numPointLights);
	void QueueVolumetricFog(RdrAction* pAction, const AssetLib::VolumetricFogSettings& rFogSettings, const RdrResourceHandle hLightIndicesRes);

private:
	// Lighting render resources
	RdrResourceHandle m_hShadowMapTexArray;
	RdrResourceHandle m_hShadowCubeMapTexArray;
	RdrDepthStencilViewHandle m_shadowMapDepthViews[MAX_SHADOW_MAPS];
#if USE_SINGLEPASS_SHADOW_CUBEMAP
	RdrDepthStencilViewHandle m_shadowCubeMapDepthViews[MAX_SHADOW_CUBEMAPS];
#else
	RdrDepthStencilViewHandle m_shadowCubeMapDepthViews[MAX_SHADOW_CUBEMAPS * (int)CubemapFace::Count];
#endif

	// Lighting mode data.
	RdrTiledLightingData m_tiledLightData;
	RdrClusteredLightingData m_clusteredLightData;

	// Volumetric fog resources
	RdrConstantBufferHandle m_hFogConstants;
	RdrResourceHandle m_hFogDensityLightLut;
	RdrResourceHandle m_hFogFinalLut;
	UVec3 m_fogLutSize;
};

