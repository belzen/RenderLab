#pragma once

struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11SamplerState;
struct ID3D11Buffer;

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
	Opaque,
	Sky,
	Alpha,
	UI,

	Count
};

enum class RdrBucketType
{
	LightCulling,
	Opaque,
	Sky,
	Alpha,
	UI,

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
	D16,
	D24_UNORM_S8_UINT,
	R16_UNORM,
	R16G16_FLOAT,
	R8_UNORM,
	DXT1,
	DXT5,
	DXT5_sRGB,
	B8G8R8A8_UNORM,
	B8G8R8A8_UNORM_sRGB,
	R16G16B16A16_FLOAT,

	Count
};

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

	Count,
};

struct RdrRasterState
{
	uint bEnableMSAA : 1;
	uint bEnableScissor : 1;
	uint bWireframe : 1;
};

struct RdrSamplerState
{
	RdrSamplerState()
		: cmpFunc((uint)RdrComparisonFunc::Never), texcoordMode((uint)RdrTexCoordMode::Wrap), bPointSample(false) {}
	RdrSamplerState(const RdrComparisonFunc cmpFunc, const RdrTexCoordMode texcoordMode, const bool bPointSample)
		: cmpFunc((uint)cmpFunc), texcoordMode((uint)texcoordMode), bPointSample(bPointSample) {}

	uint cmpFunc : 4;
	uint texcoordMode : 2;
	uint bPointSample : 1;
};

typedef uint16 RdrDepthStencilViewHandle;
struct RdrDepthStencilView
{
	ID3D11DepthStencilView* pView;
};

typedef uint16 RdrRenderTargetViewHandle;
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
