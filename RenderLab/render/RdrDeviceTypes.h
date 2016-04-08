#pragma once

struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11SamplerState;
struct ID3D11Buffer;

enum RdrShaderSemantic
{
	kRdrShaderSemantic_Position,
	kRdrShaderSemantic_Texcoord,
	kRdrShaderSemantic_Color,
	kRdrShaderSemantic_Normal,
	kRdrShaderSemantic_Binormal,
	kRdrShaderSemantic_Tangent,

	kRdrShaderSemantic_Count
};

enum RdrVertexInputFormat
{
	kRdrVertexInputFormat_RG_F32,
	kRdrVertexInputFormat_RGB_F32,
	kRdrVertexInputFormat_RGBA_F32,

	kRdrVertexInputFormat_Count
};

enum RdrVertexInputClass
{
	kRdrVertexInputClass_PerVertex,
	kRdrVertexInputClass_PerInstance,

	kRdrVertexInputClass_Count
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

enum RdrPassEnum
{
	kRdrPass_ZPrepass,
	kRdrPass_LightCulling,
	kRdrPass_Opaque,
	kRdrPass_Alpha,
	kRdrPass_UI,

	kRdrPass_Count
};

enum RdrBucketType
{
	kRdrBucketType_LightCulling,
	kRdrBucketType_Opaque,
	kRdrBucketType_Alpha,
	kRdrBucketType_UI,

	kRdrBucketType_Count
};

enum RdrShaderMode
{
	kRdrShaderMode_Normal,
	kRdrShaderMode_DepthOnly,

	kRdrShaderMode_Count
};

enum RdrTexCoordMode
{
	kRdrTexCoordMode_Wrap,
	kRdrTexCoordMode_Clamp,
	kRdrTexCoordMode_Mirror,

	kRdrTexCoordMode_Count
};

enum RdrResourceFormat
{
	kResourceFormat_D16,
	kResourceFormat_D24_UNORM_S8_UINT,
	kResourceFormat_R16_UNORM,
	kResourceFormat_RG_F16,
	kResourceFormat_R8_UNORM,
	kResourceFormat_DXT1,
	kResourceFormat_DXT5,
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

enum RdrDepthTestMode
{
	kRdrDepthTestMode_None,
	kRdrDepthTestMode_Less,
	kRdrDepthTestMode_Equal,

	kRdrDepthTestMode_Count,
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
		: cmpFunc(kComparisonFunc_Never), texcoordMode(kRdrTexCoordMode_Wrap), bPointSample(false) {}
	RdrSamplerState(const RdrComparisonFunc cmpFunc, const RdrTexCoordMode texcoordMode, const bool bPointSample)
		: cmpFunc(cmpFunc), texcoordMode(texcoordMode), bPointSample(bPointSample) {}

	uint cmpFunc : 4;
	uint texcoordMode : 2;
	uint bPointSample : 1;
};

union RdrConstantBufferDeviceObj
{
	ID3D11Buffer* pBufferD3D11;
};

typedef uint16 RdrConstantBufferHandle;
struct RdrConstantBuffer
{
	static inline uint GetRequiredSize(uint size) { return sizeof(Vec4) * (uint)ceilf(size / (float)sizeof(Vec4)); }

	RdrConstantBufferDeviceObj bufferObj;
	uint size;
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

enum RdrResourceMapMode
{
	kRdrResourceMap_Read,
	kRdrResourceMap_Write,
	kRdrResourceMap_ReadWrite,
	kRdrResourceMap_WriteDiscard,
	kRdrResourceMap_WriteNoOverwrite,

	kRdrResourceMap_Count
};

typedef uint RdrCpuAccessFlags;
enum RdrCpuAccessFlag
{
	kRdrCpuAccessFlag_Read = 0x1,
	kRdrCpuAccessFlag_Write = 0x2
};

enum RdrResourceUsage
{
	kRdrResourceUsage_Default,
	kRdrResourceUsage_Immutable,
	kRdrResourceUsage_Dynamic,
	kRdrResourceUsage_Staging,

	kRdrResourceUsage_Count
};
