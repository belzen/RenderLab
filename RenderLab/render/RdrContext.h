#pragma once

#include <dxgiformat.h>
#include <map>
#include "Math.h"
#include "Shaders.h"
#include "FreeList.h"
#include "Camera.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11DepthStencilView;
struct ID3D11View;
struct ID3D11Resource;
struct ID3D11InputLayout;
struct ID3D11SamplerState;
struct D3D11_INPUT_ELEMENT_DESC;

#define MAX_TEXTURES 1024
#define MAX_SHADERS 1024
#define MAX_GEO 1024

#define MAX_TEXTURES_PER_DRAW 16

struct Vertex
{
	Vec3 position;
	Vec3 normal;
	Color color;
	Vec2 texcoord;
	Vec3 tangent;
	Vec3 bitangent;
};

enum RdrResourceFormat
{
	kResourceFormat_D16,
	kResourceFormat_RG_F16,
	kResourceFormat_Count,
};

enum RdrComparisonFunc
{
	kComparisonFunc_Never,
	kComparisonFunc_Less,
	kComparisonFunc_Equal,
	kComparisonFunc_LessEqual,
	kComparisonFunc_Greater,
	kComparisonFunc_NotEqual,
	kComparisonFunc_GreaterEqual,
	kComparisonFunc_Always,

	kComparisonFunc_Count
};

struct RdrResource
{
	union
	{
		ID3D11Texture2D* pTexture;
		ID3D11Resource* pResource;
	};

	// All optional.
	ID3D11ShaderResourceView*	pResourceView;
	ID3D11UnorderedAccessView*	pUnorderedAccessView;

	int width;
	int height;
};

struct RdrGeometry
{
	ID3D11Buffer* pVertexBuffer;
	ID3D11Buffer* pIndexBuffer;
	int numVerts;
	int numIndices;
	uint vertStride;
	Vec3 size;
	float radius;
};

enum RdrTexCoordMode
{
	kRdrTexCoordMode_Wrap,
	kRdrTexCoordMode_Clamp,
	kRdrTexCoordMode_Mirror,

	kRdrTexCoordMode_Count
};

struct RdrSamplerState
{
	RdrSamplerState() 
		: cmpFunc(kComparisonFunc_Never), texcoordMode(kRdrTexCoordMode_Wrap), bPointSample(false) {}
	RdrSamplerState(const RdrComparisonFunc cmpFunc, const RdrTexCoordMode texcoordMode, const bool bPointSample)
		: cmpFunc(cmpFunc), texcoordMode(texcoordMode), bPointSample(bPointSample) {}

	uint cmpFunc : 4;
	uint texcoordMode : 2;
	uint bPointSample : 1;
};

struct RdrSampler
{
	ID3D11SamplerState* pSampler;
};

struct RdrDepthStencilView
{
	ID3D11DepthStencilView* pView;
};

#define SAMPLER_TYPES_COUNT kComparisonFunc_Count * kRdrTexCoordMode_Count * 2

typedef FreeList<RdrResource, MAX_TEXTURES> RdrResourceList;
typedef FreeList<VertexShader, MAX_SHADERS> VertexShaderList;
typedef FreeList<PixelShader, MAX_SHADERS> PixelShaderList;
typedef FreeList<ComputeShader, MAX_SHADERS> ComputeShaderList;
typedef FreeList<RdrGeometry, MAX_GEO> RdrGeoList;
typedef uint RdrGeoHandle;
typedef uint RdrResourceHandle;
typedef uint ShaderHandle;
typedef std::map<std::string, RdrResourceHandle> TextureMap;
typedef std::map<std::string, ShaderHandle> ShaderMap;
typedef std::map<std::string, RdrGeoHandle> GeoMap;

class RdrContext
{
public:
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;

	Camera m_mainCamera;

	RdrSampler m_samplers[SAMPLER_TYPES_COUNT];

	TextureMap m_textureCache;
	RdrResourceList m_resources;

	ShaderMap m_vertexShaderCache;
	VertexShaderList m_vertexShaders;

	ShaderMap m_pixelShaderCache;
	PixelShaderList m_pixelShaders;

	ComputeShaderList m_computeShaders;

	GeoMap m_geoCache;
	RdrGeoList m_geo;

	RdrResourceHandle m_hTileLightIndices;

	void InitSamplers();
	RdrSampler GetSampler(const RdrSamplerState& state);

	RdrGeoHandle LoadGeo(const char* filename);
	RdrGeoHandle CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size);

	RdrResourceHandle LoadTexture(const char* filename);
	void ReleaseResource(RdrResourceHandle hTex);

	RdrResourceHandle CreateTexture2D(uint width, uint height, RdrResourceFormat format);
	RdrResourceHandle CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat format);

	RdrDepthStencilView CreateDepthStencilView(RdrResourceHandle hTexArrayRes, int arrayIndex);

	ShaderHandle LoadVertexShader(const char* filename, D3D11_INPUT_ELEMENT_DESC* aDesc, int numElements);
	ShaderHandle LoadPixelShader(const char* filename);
	ShaderHandle LoadComputeShader(const char* filename);

	RdrResourceHandle CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize);

	ID3D11Buffer* CreateVertexBuffer(const void* vertices, int size);
	ID3D11Buffer* CreateIndexBuffer(const void* indices, int size);
};

enum RdrPassEnum
{
	kRdrPass_ZPrepass,
	kRdrPass_Opaque,
	kRdrPass_Alpha,
	kRdrPass_UI,
	kRdrPass_Count
};

enum RdrBucketType
{
	kRdrBucketType_Opaque,
	kRdrBucketType_Alpha,
	kRdrBucketType_UI,
	kRdrBucketType_Count
};

struct RdrDrawOp
{
	static RdrDrawOp* Allocate();
	static void Release(RdrDrawOp* pDrawOp);

	Vec3 position;

	RdrSamplerState samplers[MAX_TEXTURES_PER_DRAW];
	RdrResourceHandle hTextures[MAX_TEXTURES_PER_DRAW];
	uint texCount;

	// UAVs
	RdrResourceHandle hViews[8];
	uint viewCount;

	Vec4 constants[16];
	uint numConstants;

	ShaderHandle hVertexShader;
	ShaderHandle hPixelShader;

	RdrGeoHandle hGeo;
	bool bFreeGeo;

	ShaderHandle hComputeShader;
	int computeThreads[3];

	bool needsLighting;
};
