#pragma once

#include "UtilsLib\Util.h"

#include <wrl.h>
#include "d3dx12.h"
using namespace Microsoft::WRL;

typedef CD3DX12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorHandle;
struct RdrResource;

enum class RdrShaderSemantic
{
	Position,
	Texcoord,
	Color,
	Normal,
	Binormal,
	Tangent,

	Count
};

enum class RdrVertexInputFormat
{
	R_F32,
	RG_F32,
	RGB_F32,
	RGBA_F32,

	Count
};

enum class RdrVertexInputClass
{
	PerVertex,
	PerInstance,

	Count
};

struct RdrVertexInputElement
{
	RdrShaderSemantic semantic;
	uint semanticIndex;
	RdrVertexInputFormat format;
	uint streamIndex;
	uint byteOffset;
	RdrVertexInputClass inputClass;
	uint instanceDataStepRate;
};

enum class RdrPass
{
	ZPrepass,
	LightCulling,
	VolumetricFog,
	Opaque,
	Decal,
	Sky,
	Alpha,
	Editor,
	Wireframe,
	UI,

	Count
};

enum class RdrBucketType
{
	LightCulling,
	VolumetricFog,
	Opaque,
	Decal,
	Sky,
	Alpha,
	Editor,
	Wireframe,
	UI,

	ShadowMap0,
	ShadowMap1,
	ShadowMap2,
	ShadowMap3,
	ShadowMap4,
	ShadowMap5,
	ShadowMap6,
	ShadowMap7,
	ShadowMap8,
	ShadowMap9,
	ShadowMap10,
	ShadowMap11,

	Count
};

enum class RdrShaderMode
{
	Normal,
	DepthOnly,
	ShadowMap,
	Wireframe,

	Count
};

enum class RdrTexCoordMode
{
	Wrap,
	Clamp,
	Mirror,

	Count
};

enum class RdrResourceFormat
{
	UNKNOWN,
	D16,
	D24_UNORM_S8_UINT,
	R16_UNORM,
	R16_UINT,
	R16_FLOAT,
	R32_UINT,
	R16G16_FLOAT,
	R8_UNORM,
	DXT1,
	DXT1_sRGB,
	DXT5,
	DXT5_sRGB,
	B8G8R8A8_UNORM,
	B8G8R8A8_UNORM_sRGB,
	R8G8B8A8_UNORM,
	R8G8_UNORM,
	R16G16B16A16_FLOAT,

	Count
};

int rdrGetTexturePitch(const int width, const RdrResourceFormat eFormat);
int rdrGetTextureRows(const int height, const RdrResourceFormat eFormat);


enum class RdrComparisonFunc
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always,

	Count
};

enum class RdrBlendMode
{
	kOpaque,
	kAlpha,
	kAdditive,
	kSubtractive,

	kCount
};

struct RdrDepthStencilState
{
	RdrDepthStencilState(bool bTest, bool bWrite, RdrComparisonFunc eFunc)
		: bTestDepth(bTest), bWriteDepth(bWrite), eDepthFunc(eFunc) {}

	bool bTestDepth;
	bool bWriteDepth;
	RdrComparisonFunc eDepthFunc;
};

struct RdrRasterState
{
	uint bEnableMSAA : 1;
	uint bEnableScissor : 1;
	uint bWireframe : 1;
	uint bUseSlopeScaledDepthBias : 1; // Uses global slope scaled depth bias
	uint bDoubleSided : 1;
};

struct RdrSamplerState
{
	RdrSamplerState()
		: cmpFunc((uint)RdrComparisonFunc::Never), texcoordMode((uint)RdrTexCoordMode::Wrap), bPointSample(false), unused(0) {}
	RdrSamplerState(const RdrComparisonFunc cmpFunc, const RdrTexCoordMode texcoordMode, const bool bPointSample)
		: cmpFunc((uint)cmpFunc), texcoordMode((uint)texcoordMode), bPointSample(bPointSample), unused(0) {}

	union
	{
		struct  
		{
			uint cmpFunc		: 4;
			uint texcoordMode	: 2;
			uint bPointSample	: 1;
			uint unused			: 25;
		};

		uint compareVal; // For quick comparison tests.
	};
};

struct RdrRootSignature
{
	ComPtr<ID3D12RootSignature> pRootSignature;
};

struct RdrPipelineState
{
	ID3D12PipelineState* pPipelineState;
};

struct RdrDepthStencilView
{
	D3D12DescriptorHandle hView;
	RdrResource* pResource;
};

struct RdrRenderTargetView
{
	D3D12DescriptorHandle hView;
	RdrResource* pResource;
};

enum class RdrResourceMapMode
{
	Read,
	Write,
	ReadWrite,
	WriteDiscard,
	WriteNoOverwrite,

	Count
};

enum class RdrResourceAccessFlags
{
	None     = 0x0,

	CpuRead  = 0x1,
	CpuWrite = 0x2,

	GpuRead  = 0x4,
	GpuWrite = 0x8,
	GpuRenderTarget = 0x10,

	GpuRW = GpuRead | GpuWrite,
	CpuRO_GpuRW = CpuRead | GpuRead | GpuWrite,
	CpuRW_GpuRO = CpuRead | CpuWrite | GpuRead,
	CpuRO_GpuRO = CpuRead | GpuRead,
	CpuRO_GpuRO_RenderTarget = CpuRead | GpuRead | GpuRenderTarget,
};
ENUM_FLAGS(RdrResourceAccessFlags);

struct RdrBox
{
	RdrBox()
		: left(0), top(0), front(0), width(0), height(0), depth(0) {}
	RdrBox(uint left, uint top, uint front, uint width, uint height, uint depth)
		: left(left), top(top), front(front), width(width), height(height), depth(depth) {}

	uint left;
	uint top;
	uint front;
	uint width;
	uint height;
	uint depth;
};

struct RdrQuery
{
	uint nId;
};

//////////////////////////////////////////////////////////////////////////

inline bool operator == (const RdrSamplerState& rLeft, const RdrSamplerState& rRight)
{
	return rLeft.compareVal == rRight.compareVal;
}

inline bool operator != (const RdrSamplerState& rLeft, const RdrSamplerState& rRight)
{
	return !(rLeft == rRight);
}