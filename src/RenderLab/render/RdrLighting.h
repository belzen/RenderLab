#pragma once

#include "Math.h"
#include "RdrContext.h"
#include "AssetLib/SceneAsset.h"
#include "RdrShaderTypes.h"
#include "UtilsLib/FixedVector.h"
#include "../../data/shaders/light_types.h"

struct ID3D11Buffer;
struct ID3D11UnorderedAccessView;
class RdrContext;
class Renderer;
class Camera;
class Scene;
class Sky;
class Light;

#define MAX_SHADOW_MAPS_PER_FRAME 10
#define USE_SINGLEPASS_SHADOW_CUBEMAP 1

enum class RdrLightingMethod
{
	Tiled,
	Clustered
};

struct RdrTiledLightingData
{
	RdrResourceHandle       hLightIndices;
	RdrConstantBufferHandle hDepthMinMaxConstants;
	RdrConstantBufferHandle hCullConstants;
	RdrResourceHandle	    hDepthMinMaxTex;
	int					    tileCountX;
	int					    tileCountY;
};

struct RdrClusteredLightingData
{
	RdrResourceHandle       hLightIndices;
	RdrConstantBufferHandle hCullConstants;
	int					    clusterCountX;
	int					    clusterCountY;
	int					    clusterCountZ;
	int						clusterTileSize;
};

struct RdrLightResources
{
	RdrConstantBufferHandle hGlobalLightsCb;
	RdrResourceHandle hSpotLightListRes;
	RdrResourceHandle hPointLightListRes;
	RdrResourceHandle hEnvironmentLightListRes;
	RdrResourceHandle hLightIndicesRes;

	RdrResourceHandle hSkyTransmittanceLut;
	RdrResourceHandle hVolumetricFogLut;
	RdrResourceHandle hEnvironmentMapTexArray;
	RdrResourceHandle hShadowMapTexArray;
	RdrResourceHandle hShadowCubeMapTexArray;
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

	void Init();
	void Cleanup();

	void QueueDraw(RdrLightList* pLights, Renderer& rRenderer, const Sky& rSky, const Camera& rCamera, const float sceneDepthMin, const float sceneDepthMax,
		RdrLightingMethod lightingMethod, RdrLightResources* pOutResources);

private:
	void QueueClusteredLightCulling(Renderer& rRenderer, const Camera& rCamera, int numSpotLights, int numPointLights);
	void QueueTiledLightCulling(Renderer& rRenderer, const Camera& rCamera, int numSpotLights, int numPointLights);

private:
	// Render resources
	RdrConstantBufferHandle m_hGlobalLightsCb;
	RdrResourceHandle m_hSpotLightListRes;
	RdrResourceHandle m_hPointLightListRes;
	RdrResourceHandle m_hEnvironmentLightListRes;
	RdrResourceHandle m_hLightIndicesRes;

	RdrResourceHandle m_hShadowMapTexArray;
	RdrResourceHandle m_hShadowCubeMapTexArray;
	RdrDepthStencilViewHandle m_shadowMapDepthViews[MAX_SHADOW_MAPS];
#if USE_SINGLEPASS_SHADOW_CUBEMAP
	RdrDepthStencilViewHandle m_shadowCubeMapDepthViews[MAX_SHADOW_CUBEMAPS];
#else
	RdrDepthStencilViewHandle m_shadowCubeMapDepthViews[MAX_SHADOW_CUBEMAPS * CubemapFace::Count];
#endif

	// Lighting mode data.
	RdrTiledLightingData m_tiledLightData;
	RdrClusteredLightingData m_clusteredLightData;
};

