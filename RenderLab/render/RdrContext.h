#pragma once

#include <map>
#include "RdrHandles.h"
#include "RdrDeviceObjects.h"
#include "Math.h"
#include "RdrShaders.h"
#include "FreeList.h"
#include "Camera.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11View;
struct ID3D11Resource;
struct ID3D11InputLayout;
struct ID3D11SamplerState;
struct D3D11_INPUT_ELEMENT_DESC;

#define MAX_TEXTURES 1024
#define MAX_SHADERS 1024
#define MAX_GEO 1024

struct Vertex
{
	Vec3 position;
	Vec3 normal;
	Color color;
	Vec2 texcoord;
	Vec3 tangent;
	Vec3 bitangent;
};

#define SAMPLER_TYPES_COUNT kComparisonFunc_Count * kRdrTexCoordMode_Count * 2

typedef FreeList<RdrResource, MAX_TEXTURES> RdrResourceList;
typedef FreeList<VertexShader, MAX_SHADERS> VertexShaderList;
typedef FreeList<PixelShader, MAX_SHADERS> PixelShaderList;
typedef FreeList<ComputeShader, MAX_SHADERS> ComputeShaderList;
typedef FreeList<RdrGeometry, MAX_GEO> RdrGeoList;
typedef std::map<std::string, RdrResourceHandle> TextureMap;
typedef std::map<std::string, ShaderHandle> ShaderMap;
typedef std::map<std::string, RdrGeoHandle> GeoMap;

class RdrContext
{
public:
	void InitSamplers();
	RdrSampler GetSampler(const RdrSamplerState& state);

	RdrGeoHandle LoadGeo(const char* filename);
	RdrGeoHandle CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size);

	RdrResourceHandle LoadTexture(const char* filename);
	void ReleaseResource(RdrResourceHandle hTex);

	RdrResourceHandle CreateTexture2D(uint width, uint height, RdrResourceFormat format);
	RdrResourceHandle CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat format);

	RdrResourceHandle CreateTextureCube(uint width, uint height, RdrResourceFormat format);
	RdrResourceHandle CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat format);

	RdrDepthStencilView CreateDepthStencilView(RdrResourceHandle hTexArrayRes, int arrayIndex);
	RdrRenderTargetView CreateRenderTargetView(RdrResourceHandle hTexArrayRes, int arrayIndex, RdrResourceFormat format);

	ShaderHandle LoadVertexShader(const char* filename, D3D11_INPUT_ELEMENT_DESC* aDesc, int numElements);
	ShaderHandle LoadPixelShader(const char* filename);
	ShaderHandle LoadComputeShader(const char* filename);

	RdrResourceHandle CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize);

	ID3D11Buffer* CreateVertexBuffer(const void* vertices, int size);
	ID3D11Buffer* CreateIndexBuffer(const void* indices, int size);

//private:
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;

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
};

