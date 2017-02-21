#pragma once

#include "UtilsLib\Util.h"

struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11SamplerState;
struct ID3D11Buffer;
struct ID3D11Query;

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

	Count
};

enum class RdrShaderMode
{
	Normal,
	DepthOnly,

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

enum class RdrDepthTestMode
{
	None,
	Less,
	Equal,

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

struct RdrDepthStencilView
{
	ID3D11DepthStencilView* pView;
};

struct RdrRenderTargetView
{
	ID3D11RenderTargetView* pView;
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

enum class RdrCpuAccessFlags
{
	None = 0x0,
	Read = 0x1,
	Write = 0x2
};
ENUM_FLAGS(RdrCpuAccessFlags);

enum class RdrResourceUsage
{
	Default,
	Immutable,
	Dynamic,
	Staging,

	Count
};

enum class RdrResourceBindings
{
	kNone,
	kUnorderedAccessView,
	kRenderTarget,
};

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

enum class RdrQueryType : uint
{
	Timestamp,
	Disjoint
};

struct RdrQueryDisjointData
{
	uint64 frequency;
	bool isDisjoint;
};

struct RdrQuery
{
	union
	{
		ID3D11Query* pQueryD3D11;
		void* pTypeless;
	};
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