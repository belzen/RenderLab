#pragma once
#include "RdrEnums.h"

struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11Resource;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11SamplerState;

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

struct RdrRenderTargetView
{
	ID3D11RenderTargetView* pView;
};