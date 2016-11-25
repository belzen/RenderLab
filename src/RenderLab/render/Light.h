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

class LightList
{
public:
	LightList();

	void Init(Scene* pScene);
	void Cleanup();

	void AddDirectionalLight(const Vec3& direction, const Vec3& color);
	void AddSpotLight(const Vec3& position, const Vec3& direction, const Vec3& color, float radius, float innerConeAngle, float outerConeAngle);
	void AddPointLight(const Vec3& position, const Vec3& color, float radius);
	void AddEnvironmentLight(const Vec3& position);
	void SetGlobalEnvironmentLight(const Vec3& position);

	void InvalidateEnvironmentLights();

	void QueueDraw(Renderer* pRenderer, const Sky& rSky, const Camera& rCamera, const float sceneDepthMin, const float sceneDepthMax,
		RdrLightingMethod lightingMethod, RdrLightResources* pOutResources);

private:
	void QueueClusteredLightCulling(Renderer* pRenderer, const Camera& rCamera, const RdrLightResources& rLightResources);
	void QueueTiledLightCulling(Renderer* pRenderer, const Camera& rCamera, const RdrLightResources& rLightResources);

private:
	// Light lists
	FixedVector<DirectionalLight, 4> m_directionalLights;
	FixedVector<PointLight, 256> m_pointLights;
	FixedVector<SpotLight, 256> m_spotLights;
	FixedVector<EnvironmentLight, 64> m_environmentLights;
	EnvironmentLight m_globalEnvironmentLight;

	// Render resources
	RdrResourceHandle m_hEnvironmentMapTexArray;
	RdrResourceHandle m_hEnvironmentMapDepthBuffer;
	RdrDepthStencilViewHandle m_hEnvironmentMapDepthView;

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

	Scene* m_pScene; // Scene the lights belong to.  Used for environment light captures.
};

