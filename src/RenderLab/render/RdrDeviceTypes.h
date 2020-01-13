#pragma once

#include "UtilsLib\Util.h"
#include "UtilsLib\Error.h"
#include "RdrDebugBackpointer.h"
#include "AssetLib\ModelAsset.h"

#include <wrl.h>
#include "d3dx12.h"
using namespace Microsoft::WRL;

struct RdrResource;
class RdrContext;
struct RdrShader;

enum class RdrDescriptorType
{
	SRV,
	UAV,
	CBV,
	Sampler,
	RTV,
	DSV
};

class RdrDescriptors
{
public:
	bool IsValid() const { return m_hCpuDesc.ptr != 0; }
	void Reset() { m_hCpuDesc.ptr = 0; m_hGpuDesc.ptr = 0; }

	void MarkUsedThisFrame() const;
	uint64 GetLastUsedFrame() const { return m_nLastUsedFrameCode; }
	int GetRefCount() const { return m_nRefCount; }

	const CD3DX12_CPU_DESCRIPTOR_HANDLE& GetCpuDesc() const { return m_hCpuDesc; }
	const CD3DX12_GPU_DESCRIPTOR_HANDLE& GetGpuDesc() const { MarkUsedThisFrame(); return m_hGpuDesc; }

	const CD3DX12_CPU_DESCRIPTOR_HANDLE& GetCopyableCpuDesc() const { return m_hCpuCopyDesc; }

	RdrDescriptorType GetType() const { return m_eDescType; }

	bool Release();

private:
	friend class DescriptorHeap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuCopyDesc;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuDesc;
	CD3DX12_GPU_DESCRIPTOR_HANDLE m_hGpuDesc;

	int m_nRefCount			= 0;
	uint8 m_nTableSize		= 0;
	uint8 m_nTableListIndex	= 0;

	uint64 m_nLastUsedFrameCode = 0;
	RdrDebugBackpointer m_debugCreator;
	RdrDescriptorType m_eDescType;
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
	ZPrepass,
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
	BC5_UNORM,
	B8G8R8A8_UNORM,
	B8G8R8A8_UNORM_sRGB,
	R8G8B8A8_UNORM,
	R8G8_UNORM,
	R16G16B16A16_FLOAT,

	Count
};

uint rdrGetResourceFormatSize(RdrResourceFormat eFormat);
int rdrGetTexturePitch(const int width, const RdrResourceFormat eFormat);
int rdrGetTextureRows(const int height, const RdrResourceFormat eFormat);

enum class RdrTopology
{
	Undefined,
	TriangleList,
	TriangleStrip,
	Quad,
	Point,

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
	RdrDepthStencilState()
		: bTestDepth(false), bWriteDepth(false), eDepthFunc(RdrComparisonFunc::Never) {}
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
public:
	static RdrPipelineState* CreateGraphicsPipelineState(
		const RdrShader* pVertexShader, const RdrShader* pPixelShader,
		const RdrShader* pHullShader, const RdrShader* pDomainShader,
		const RdrVertexInputElement* pInputLayoutElements, uint nNumInputElements,
		const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
		const RdrBlendMode eBlendMode,
		const RdrRasterState& rasterState,
		const RdrDepthStencilState& depthStencilState);

	static RdrPipelineState* CreateComputePipelineState(const RdrShader* pComputeShader);

//////////////////////////////////////////////////////////////////////////
public:
	void ReCreate();
	void Release();

	void MarkUsedThisFrame() const;
	uint64 GetLastUsedFrame() const { return m_nLastUsedFrameCode; }

	ID3D12PipelineState* GetPipelineState() const { MarkUsedThisFrame(); return m_pDevPipelineState; }

private:
	ID3D12PipelineState* m_pDevPipelineState = nullptr;
	uint64 m_nLastUsedFrameCode = 0;

	// Data used to create the pipeline state.
	const RdrShader* m_pComputeShader = nullptr;
	const RdrShader* m_pVertexShader = nullptr;
	const RdrShader* m_pPixelShader = nullptr;
	const RdrShader* m_pHullShader = nullptr;
	const RdrShader* m_pDomainShader = nullptr;

	std::vector<RdrVertexInputElement> m_inputLayoutElements;
	std::vector<RdrResourceFormat> m_rtvFormats;

	RdrBlendMode m_eBlendMode;
	RdrRasterState m_rasterState;
	RdrDepthStencilState m_depthStencilState;
	bool m_bForGraphics;
};


struct RdrDepthStencilView
{
	RdrDescriptors* pDesc;
	RdrResource* pResource;
};

struct RdrRenderTargetView
{
	RdrDescriptors* pDesc;
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

inline D3D12_PRIMITIVE_TOPOLOGY getD3DTopology(const RdrTopology eTopology)
{
	static const D3D12_PRIMITIVE_TOPOLOGY s_d3dTopology[] = {
		D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,					// RdrTopology::Undefined
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,				// RdrTopology::TriangleList
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,				// RdrTopology::TriangleStrip
		D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,	// RdrTopology::Quad
		D3D_PRIMITIVE_TOPOLOGY_POINTLIST,					// RdrTopology::Point
	};
	static_assert(ARRAY_SIZE(s_d3dTopology) == (int)RdrTopology::Count, "Missing D3D topologies!");
	return s_d3dTopology[(int)eTopology];
}

inline DXGI_FORMAT getD3DFormat(const RdrResourceFormat format)
{
	static const DXGI_FORMAT s_d3dFormats[] = {
		DXGI_FORMAT_UNKNOWN,				// ResourceFormat::UNKNOWN
		DXGI_FORMAT_R16_UNORM,				// ResourceFormat::D16
		DXGI_FORMAT_R24_UNORM_X8_TYPELESS,	// ResourceFormat::D24_UNORM_S8_UINT
		DXGI_FORMAT_R16_UNORM,				// ResourceFormat::R16_UNORM
		DXGI_FORMAT_R16_UINT,				// ResourceFormat::R16_UINT
		DXGI_FORMAT_R16_FLOAT,				// ResourceFormat::R16_FLOAT
		DXGI_FORMAT_R32_UINT,				// ResourceFormat::R32_UINT
		DXGI_FORMAT_R16G16_FLOAT,			// ResourceFormat::R16G16_FLOAT
		DXGI_FORMAT_R8_UNORM,				// ResourceFormat::R8_UNORM
		DXGI_FORMAT_BC1_UNORM,				// ResourceFormat::DXT1
		DXGI_FORMAT_BC1_UNORM_SRGB,			// ResourceFormat::DXT1_sRGB
		DXGI_FORMAT_BC3_UNORM,				// ResourceFormat::DXT5
		DXGI_FORMAT_BC3_UNORM_SRGB,			// ResourceFormat::DXT5_sRGB
		DXGI_FORMAT_BC5_UNORM,				// ResourceFormat::BC5_UNORM
		DXGI_FORMAT_B8G8R8A8_UNORM,			// ResourceFormat::B8G8R8A8_UNORM
		DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,	// ResourceFormat::B8G8R8A8_UNORM_sRGB
		DXGI_FORMAT_R8G8B8A8_UNORM,			// ResourceFormat::R8G8B8A8_UNORM
		DXGI_FORMAT_R8G8_UNORM,				// ResourceFormat::R8G8_UNORM
		DXGI_FORMAT_R16G16B16A16_FLOAT,		// ResourceFormat::R16G16B16A16_FLOAT
	};
	static_assert(ARRAY_SIZE(s_d3dFormats) == (int)RdrResourceFormat::Count, "Missing D3D formats!");
	return s_d3dFormats[(int)format];
}

inline DXGI_FORMAT getD3DFormat(const RdrIndexBufferFormat eFormat)
{
	static const DXGI_FORMAT s_d3dFormats[] = {
		DXGI_FORMAT_R16_UINT,				// RdrIndexBufferFormat::R16_UINT
		DXGI_FORMAT_R32_UINT,				// RdrIndexBufferFormat::R32_UINT
	};
	static_assert(ARRAY_SIZE(s_d3dFormats) == (int)RdrIndexBufferFormat::Count, "Missing D3D formats!");
	return s_d3dFormats[(int)eFormat];
}

inline DXGI_FORMAT getD3DDepthFormat(const RdrResourceFormat format)
{
	static const DXGI_FORMAT s_d3dDepthFormats[] = {
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::UNKNOWN
		DXGI_FORMAT_D16_UNORM,			// ResourceFormat::D16
		DXGI_FORMAT_D24_UNORM_S8_UINT,	// ResourceFormat::D24_UNORM_S8_UINT
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R16_UNORM
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R16_UINT
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R16_FLOAT
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R32_UINT
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R16G16_FLOAT
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R8_UNORM
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT1
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT1_sRGB
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT5
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT5_sRGB
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::BC5_UNORM
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::B8G8R8A8_UNORM
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::B8G8R8A8_UNORM_sRGB
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R8G8B8A8_UNORM
		DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R8G8B8_UNORM
		DXGI_FORMAT_UNKNOWN,	        // ResourceFormat::R16G16B16A16_FLOAT
	};
	static_assert(ARRAY_SIZE(s_d3dDepthFormats) == (int)RdrResourceFormat::Count, "Missing D3D depth formats!");
	Assert(s_d3dDepthFormats[(int)format] != DXGI_FORMAT_UNKNOWN);
	return s_d3dDepthFormats[(int)format];
}

inline DXGI_FORMAT getD3DTypelessFormat(const RdrResourceFormat format)
{
	static const DXGI_FORMAT s_d3dTypelessFormats[] = {
		DXGI_FORMAT_UNKNOWN,			   // ResourceFormat::UNKNOWN
		DXGI_FORMAT_R16_TYPELESS,		   // ResourceFormat::D16
		DXGI_FORMAT_R24G8_TYPELESS,		   // ResourceFormat::D24_UNORM_S8_UINT
		DXGI_FORMAT_R16_TYPELESS,		   // ResourceFormat::R16_UNORM
		DXGI_FORMAT_R16_TYPELESS,		   // ResourceFormat::R16_UINT
		DXGI_FORMAT_R16_TYPELESS,		   // ResourceFormat::R16_FLOAT
		DXGI_FORMAT_R32_TYPELESS,          // ResourceFormat::R32_UINT
		DXGI_FORMAT_R16G16_TYPELESS,	   // ResourceFormat::R16G16_FLOAT
		DXGI_FORMAT_R8_TYPELESS,		   // ResourceFormat::R8_UNORM
		DXGI_FORMAT_BC1_TYPELESS,		   // ResourceFormat::DXT1
		DXGI_FORMAT_BC1_TYPELESS,		   // ResourceFormat::DXT1_sRGB
		DXGI_FORMAT_BC3_TYPELESS,		   // ResourceFormat::DXT5
		DXGI_FORMAT_BC3_TYPELESS,		   // ResourceFormat::DXT5_sRGB
		DXGI_FORMAT_BC5_TYPELESS,		   // ResourceFormat::BC5_UNORM
		DXGI_FORMAT_B8G8R8A8_TYPELESS,	   // ResourceFormat::B8G8R8A8_UNORM
		DXGI_FORMAT_B8G8R8A8_TYPELESS,     // ResourceFormat::B8G8R8A8_UNORM_sRGB
		DXGI_FORMAT_R8G8B8A8_TYPELESS,	   // ResourceFormat::R8G8B8A8_UNORM
		DXGI_FORMAT_R8G8_TYPELESS,	       // ResourceFormat::R8G8_UNORM
		DXGI_FORMAT_R16G16B16A16_TYPELESS, // ResourceFormat::R16G16B16A16_FLOAT
	};
	static_assert(ARRAY_SIZE(s_d3dTypelessFormats) == (int)RdrResourceFormat::Count, "Missing typeless formats!");
	return s_d3dTypelessFormats[(int)format];
}

inline const char* getD3DSemanticName(RdrShaderSemantic eSemantic)
{
	static const char* s_d3dSemantics[] = {
		"POSITION", // RdrShaderSemantic::Position
		"TEXCOORD", // RdrShaderSemantic::Texcoord
		"COLOR",    // RdrShaderSemantic::Color
		"NORMAL",   // RdrShaderSemantic::Normal
		"BINORMAL", // RdrShaderSemantic::Binormal
		"TANGENT"   // RdrShaderSemantic::Tangent
	};
	static_assert(ARRAY_SIZE(s_d3dSemantics) == (int)RdrShaderSemantic::Count, "Missing D3D12 shader semantic!");
	return s_d3dSemantics[(int)eSemantic];
}

inline D3D12_INPUT_CLASSIFICATION getD3DVertexInputClass(RdrVertexInputClass eClass)
{
	static const D3D12_INPUT_CLASSIFICATION s_d3dClasses[] = {
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,		// RdrVertexInputClass::PerVertex
		D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA	// RdrVertexInputClass::PerInstance
	};
	static_assert(ARRAY_SIZE(s_d3dClasses) == (int)RdrVertexInputClass::Count, "Missing D3D12 vertex input class!");
	return s_d3dClasses[(int)eClass];
}

inline DXGI_FORMAT getD3DVertexInputFormat(RdrVertexInputFormat eFormat)
{
	static const DXGI_FORMAT s_d3dFormats[] = {
		DXGI_FORMAT_R32_FLOAT,			// RdrVertexInputFormat::R_F32
		DXGI_FORMAT_R32G32_FLOAT,		// RdrVertexInputFormat::RG_F32
		DXGI_FORMAT_R32G32B32_FLOAT,	// RdrVertexInputFormat::RGB_F32
		DXGI_FORMAT_R32G32B32A32_FLOAT,	// RdrVertexInputFormat::RGBA_F32
		DXGI_FORMAT_R8G8B8A8_UINT,		// RdrVertexInputFormat::RGBA_U8
	};
	static_assert(ARRAY_SIZE(s_d3dFormats) == (int)RdrVertexInputFormat::Count, "Missing D3D12 vertex input format!");
	return s_d3dFormats[(int)eFormat];
}

inline bool ValidateHResult(HRESULT hResult, const char* func, const char* msg, const ComPtr<ID3DBlob>& pError = nullptr)
{
	if (FAILED(hResult))
	{
		char str[256];
		if (pError)
		{
			sprintf_s(str, "%s\n\tHRESULT: %d\n\tError: %s\n\tFunction: %s", msg, hResult, (const char*)pError->GetBufferPointer(), func);
		}
		else
		{
			sprintf_s(str, "%s\n\tHRESULT: %d\n\tFunction: %s", msg, hResult, func);
		}
		Error(str);
		return false;
	}

	return true;
}

inline D3D12_COMPARISON_FUNC getComparisonFuncD3d(const RdrComparisonFunc cmpFunc)
{
	D3D12_COMPARISON_FUNC cmpFuncD3d[] = {
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_COMPARISON_FUNC_LESS,
		D3D12_COMPARISON_FUNC_EQUAL,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_COMPARISON_FUNC_GREATER,
		D3D12_COMPARISON_FUNC_NOT_EQUAL,
		D3D12_COMPARISON_FUNC_GREATER_EQUAL,
		D3D12_COMPARISON_FUNC_ALWAYS
	};
	static_assert(ARRAY_SIZE(cmpFuncD3d) == (int)RdrComparisonFunc::Count, "Missing comparison func");
	return cmpFuncD3d[(int)cmpFunc];
}

inline D3D12_TEXTURE_ADDRESS_MODE getTexCoordModeD3d(const RdrTexCoordMode uvMode)
{
	D3D12_TEXTURE_ADDRESS_MODE uvModeD3d[] = {
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_MIRROR
	};
	static_assert(ARRAY_SIZE(uvModeD3d) == (int)RdrTexCoordMode::Count, "Missing tex coord mode");
	return uvModeD3d[(int)uvMode];
}

inline D3D12_FILTER getFilterD3d(const RdrComparisonFunc cmpFunc, const bool bPointSample)
{
	if (cmpFunc != RdrComparisonFunc::Never)
	{
		return bPointSample ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	}
	else
	{
		return bPointSample ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	}
}
