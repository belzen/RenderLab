#pragma once

#include <dxgiformat.h>
#include <map>
#include "Math.h"
#include "Shaders.h"
#include "FreeList.h"
#include "Light.h"
#include "Camera.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
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

struct RdrTexture
{
	union
	{
		ID3D11Texture2D* pTexture;
		ID3D11Resource* pResource;
	};

	// All optional.
	ID3D11ShaderResourceView*	pResourceView;
	ID3D11UnorderedAccessView*	pUnorderedAccessView;
	ID3D11SamplerState*			pSamplerState;

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

typedef FreeList<RdrTexture, MAX_TEXTURES> RdrTextureList;
typedef FreeList<VertexShader, MAX_SHADERS> VertexShaderList;
typedef FreeList<PixelShader, MAX_SHADERS> PixelShaderList;
typedef FreeList<ComputeShader, MAX_SHADERS> ComputeShaderList;
typedef FreeList<RdrGeometry, MAX_GEO> RdrGeoList;
typedef uint RdrGeoHandle;
typedef uint RdrTextureHandle;
typedef uint ShaderHandle;
typedef std::map<std::string, RdrTextureHandle> TextureMap;
typedef std::map<std::string, ShaderHandle> ShaderMap;
typedef std::map<std::string, RdrGeoHandle> GeoMap;

struct RdrContext
{
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;

	Camera m_mainCamera;

	TextureMap m_textureCache;
	RdrTextureList m_textures;

	ShaderMap m_vertexShaderCache;
	VertexShaderList m_vertexShaders;

	ShaderMap m_pixelShaderCache;
	PixelShaderList m_pixelShaders;

	ComputeShaderList m_computeShaders;

	GeoMap m_geoCache;
	RdrGeoList m_geo;

	LightList m_lights;
	RdrTextureHandle m_hTileLightIndices;

	RdrGeoHandle LoadGeo(const char* filename);
	RdrGeoHandle CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size);

	RdrTextureHandle LoadTexture(const char* filename, bool bPointSample);

	RdrTextureHandle CreateTexture2D(uint width, uint height, DXGI_FORMAT format);

	ShaderHandle LoadVertexShader(const char* filename, D3D11_INPUT_ELEMENT_DESC* aDesc, int numElements);
	ShaderHandle LoadPixelShader(const char* filename);
	ShaderHandle LoadComputeShader(const char* filename);

	RdrTextureHandle CreateStructuredBuffer(const void* data, int dataSize, int elementSize);

	ID3D11Buffer* CreateVertexBuffer(const void* vertices, int size);
	ID3D11Buffer* CreateIndexBuffer(const void* indices, int size);
};

enum RdrPassEnum
{
	RDRPASS_ZPREPASS,
	RDRPASS_OPAQUE,
	RDRPASS_ALPHA,
	RDRPASS_UI,
	RDRPASS_COUNT
};

enum RdrBucketType
{
	RBT_OPAQUE,
	RBT_ALPHA,
	RBT_UI,
	RBT_COUNT
};

struct RdrDrawOp
{
	static RdrDrawOp* Allocate();
	static void Release(RdrDrawOp* pDrawOp);

	RdrTextureHandle hTextures[MAX_TEXTURES_PER_DRAW];
	uint texCount;

	// UAVs
	RdrTextureHandle hViews[8];
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
