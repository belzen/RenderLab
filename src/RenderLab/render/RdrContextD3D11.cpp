#include "Precompiled.h"
#include "RdrContextD3D11.h"
#include "RdrDrawOp.h"
#include "RdrDrawState.h"
#include "RdrProfiler.h"

#include <d3d11.h>
#include <d3d11_1.h>
#pragma comment (lib, "d3d11.lib")

namespace
{
	const int kNumBackBuffers = 2;

	bool resourceFormatIsDepth(const RdrResourceFormat eFormat)
	{
		return eFormat == RdrResourceFormat::D16 || eFormat == RdrResourceFormat::D24_UNORM_S8_UINT;
	}

	bool resourceFormatIsCompressed(const RdrResourceFormat eFormat)
	{
		return eFormat == RdrResourceFormat::DXT5 || eFormat == RdrResourceFormat::DXT1;
	}

	D3D11_PRIMITIVE_TOPOLOGY getD3DTopology(const RdrTopology eTopology)
	{
		static const D3D11_PRIMITIVE_TOPOLOGY s_d3dTopology[] = {
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,				// RdrTopology::TriangleList
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,				// RdrTopology::TriangleStrip
			D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,	// RdrTopology::Quad
		};
		static_assert(ARRAY_SIZE(s_d3dTopology) == (int)RdrTopology::Count, "Missing D3D topologies!");
		return s_d3dTopology[(int)eTopology];
	}

	DXGI_FORMAT getD3DFormat(const RdrResourceFormat format)
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
			DXGI_FORMAT_B8G8R8A8_UNORM,			// ResourceFormat::B8G8R8A8_UNORM
			DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,	// ResourceFormat::B8G8R8A8_UNORM_sRGB
			DXGI_FORMAT_R8G8B8A8_UNORM,			// ResourceFormat::R8G8B8A8_UNORM
			DXGI_FORMAT_R8G8_UNORM,				// ResourceFormat::R8G8_UNORM
			DXGI_FORMAT_R16G16B16A16_FLOAT,		// ResourceFormat::R16G16B16A16_FLOAT
		};
		static_assert(ARRAY_SIZE(s_d3dFormats) == (int)RdrResourceFormat::Count, "Missing D3D formats!");
		return s_d3dFormats[(int)format];
	}

	DXGI_FORMAT getD3DDepthFormat(const RdrResourceFormat format)
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
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::B8G8R8A8_UNORM
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::B8G8R8A8_UNORM_sRGB
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R8G8B8A8_UNORM
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R8G8B8_UNORM
			DXGI_FORMAT_UNKNOWN,	        // ResourceFormat::R16G16B16A16_FLOAT
		};
		static_assert(ARRAY_SIZE(s_d3dDepthFormats) == (int)RdrResourceFormat::Count, "Missing D3D depth formats!");
		assert(s_d3dDepthFormats[(int)format] != DXGI_FORMAT_UNKNOWN);
		return s_d3dDepthFormats[(int)format];
	}

	DXGI_FORMAT getD3DTypelessFormat(const RdrResourceFormat format)
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
			DXGI_FORMAT_B8G8R8A8_TYPELESS,	   // ResourceFormat::B8G8R8A8_UNORM
			DXGI_FORMAT_B8G8R8A8_TYPELESS,     // ResourceFormat::B8G8R8A8_UNORM_sRGB
			DXGI_FORMAT_R8G8B8A8_TYPELESS,	   // ResourceFormat::R8G8B8A8_UNORM
			DXGI_FORMAT_R8G8_TYPELESS,	       // ResourceFormat::R8G8_UNORM
			DXGI_FORMAT_R16G16B16A16_TYPELESS, // ResourceFormat::R16G16B16A16_FLOAT
		};
		static_assert(ARRAY_SIZE(s_d3dTypelessFormats) == (int)RdrResourceFormat::Count, "Missing typeless formats!");
		return s_d3dTypelessFormats[(int)format];
	}

	uint getD3DCpuAccessFlags(const RdrCpuAccessFlags cpuAccessFlags)
	{
		uint d3dFlags = 0;
		if ((cpuAccessFlags & RdrCpuAccessFlags::Read) != RdrCpuAccessFlags::None)
			d3dFlags |= D3D11_CPU_ACCESS_READ;
		if ((cpuAccessFlags & RdrCpuAccessFlags::Write) != RdrCpuAccessFlags::None)
			d3dFlags |= D3D11_CPU_ACCESS_WRITE;
		return d3dFlags;
	}

	D3D11_USAGE getD3DUsage(const RdrResourceUsage eUsage)
	{
		static const D3D11_USAGE s_d3dUsage[] = {
			D3D11_USAGE_DEFAULT,	// RdrResourceUsage::Default
			D3D11_USAGE_IMMUTABLE,	// RdrResourceUsage::Immutable
			D3D11_USAGE_DYNAMIC,	// RdrResourceUsage::Dynamic
			D3D11_USAGE_STAGING,	// RdrResourceUsage::Staging
		};
		static_assert(ARRAY_SIZE(s_d3dUsage) == (int)RdrResourceUsage::Count, "Missing D3D11 resource usage!");
		return s_d3dUsage[(int)eUsage];
	}

	static void createBufferSrv(ID3D11Device* pDevice, const RdrResource& rResource, uint numElements, RdrResourceFormat eFormat, int firstElement, ID3D11ShaderResourceView** pOutView)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Buffer.FirstElement = firstElement;
		desc.Buffer.NumElements = numElements;
		desc.Format = getD3DFormat(eFormat);
		desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;

		HRESULT hr = pDevice->CreateShaderResourceView(rResource.pResource, &desc, pOutView);
		assert(hr == S_OK);
	}

	static ID3D11Buffer* createBuffer(ID3D11Device* pDevice, const void* pSrcData, const int size, const uint bindFlags)
	{
		D3D11_BUFFER_DESC desc = { 0 };
		D3D11_SUBRESOURCE_DATA data = { 0 };

		desc.Usage = (bindFlags & D3D11_BIND_UNORDERED_ACCESS) ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE;
		desc.ByteWidth = size;
		desc.BindFlags = bindFlags;
		desc.CPUAccessFlags = 0;

		data.pSysMem = pSrcData;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		ID3D11Buffer* pBuffer = nullptr;
		HRESULT hr = pDevice->CreateBuffer(&desc, (pSrcData ? &data : nullptr), &pBuffer);
		assert(hr == S_OK);

		return pBuffer;
	}
}

bool RdrContextD3D11::CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResource& rResource)
{
	D3D11_BUFFER_DESC desc = { 0 };
	D3D11_SUBRESOURCE_DATA data = { 0 };

	desc.Usage = getD3DUsage(eUsage);
	desc.ByteWidth = numElements * rdrGetTexturePitch(1, eFormat);
	desc.StructureByteStride = 0;
	desc.MiscFlags = 0;
	if (eUsage == RdrResourceUsage::Staging)
	{
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0;
	}
	else if (eUsage == RdrResourceUsage::Dynamic)
	{
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	}
	else
	{
		desc.CPUAccessFlags = 0;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	}

	data.pSysMem = pSrcData;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	HRESULT hr = m_pDevice->CreateBuffer(&desc, (pSrcData ? &data : nullptr), (ID3D11Buffer**)&rResource.pResource);
	assert(hr == S_OK);

	if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		createBufferSrv(m_pDevice, rResource, numElements, eFormat, 0, &rResource.resourceView.pViewD3D11);
	}

	if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
		desc.Buffer.FirstElement = 0;
		desc.Buffer.Flags = 0;
		desc.Buffer.NumElements = numElements;
		desc.Format = getD3DFormat(eFormat);
		desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

		hr = m_pDevice->CreateUnorderedAccessView(rResource.pResource, &desc, &rResource.uav.pViewD3D11);
		assert(hr == S_OK);
	}

	return true;
}

bool RdrContextD3D11::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage, RdrResource& rResource)
{
	D3D11_BUFFER_DESC desc = { 0 };
	D3D11_SUBRESOURCE_DATA data = { 0 };

	desc.Usage = getD3DUsage(eUsage);
	desc.ByteWidth = numElements * elementSize;
	desc.StructureByteStride = elementSize;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	if (eUsage == RdrResourceUsage::Staging)
	{
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0;
	}
	else if (eUsage == RdrResourceUsage::Dynamic)
	{
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	}
	else
	{
		desc.CPUAccessFlags = 0;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	}

	data.pSysMem = pSrcData;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	HRESULT hr = m_pDevice->CreateBuffer(&desc, (pSrcData ? &data : nullptr), (ID3D11Buffer**)&rResource.pResource);
	assert(hr == S_OK);

	if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		createBufferSrv(m_pDevice, rResource, numElements, RdrResourceFormat::UNKNOWN, 0, &rResource.resourceView.pViewD3D11);
	}

	if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		hr = m_pDevice->CreateUnorderedAccessView(rResource.pResource, nullptr, &rResource.uav.pViewD3D11);
		assert(hr == S_OK);
	}

	return true;
}

bool RdrContextD3D11::UpdateBuffer(const void* pSrcData, RdrResource& rResource, int numElements)
{
	if (numElements == -1)
	{
		numElements = rResource.bufferInfo.numElements;
	}

	if (rResource.eUsage == RdrResourceUsage::Dynamic)
	{
		D3D11_MAPPED_SUBRESOURCE mapped = { 0 };
		HRESULT hr = m_pDevContext->Map(rResource.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		assert(hr == S_OK);

		if (rResource.bufferInfo.elementSize)
		{
			memcpy(mapped.pData, pSrcData, rResource.bufferInfo.elementSize * numElements);
		}
		else
		{
			memcpy(mapped.pData, pSrcData, rdrGetTexturePitch(1, rResource.bufferInfo.eFormat) * numElements);
		}

		m_pDevContext->Unmap(rResource.pBuffer, 0);
	}
	else
	{
		D3D11_BOX box = { 0 };
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;
		box.left = 0;

		if (rResource.bufferInfo.elementSize)
		{
			box.right = rResource.bufferInfo.elementSize * numElements;
		}
		else
		{
			box.right = rdrGetTexturePitch(1, rResource.bufferInfo.eFormat) * numElements;
		}

		m_pDevContext->UpdateSubresource(rResource.pResource, 0, &box, pSrcData, 0, 0);
	}

	return true;
}

void RdrContextD3D11::CopyResourceRegion(const RdrResource& rSrcResource, const RdrBox& srcRegion, const RdrResource& rDstResource, const IVec3& dstOffset)
{
	D3D11_BOX box;
	box.left = srcRegion.left;
	box.right = srcRegion.left + srcRegion.width;
	box.top = srcRegion.top;
	box.bottom = srcRegion.top + srcRegion.height;
	box.front = srcRegion.front;
	box.back = srcRegion.front + srcRegion.depth;

	m_pDevContext->CopySubresourceRegion(rDstResource.pResource, 0, dstOffset.x, dstOffset.y, dstOffset.z, rSrcResource.pResource, 0, &box);
}

void RdrContextD3D11::ReadResource(const RdrResource& rSrcResource, void* pDstData, uint dstDataSize)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = m_pDevContext->Map(rSrcResource.pResource, 0, D3D11_MAP_READ, 0, &mapped);
	assert(hr == S_OK);

	memcpy(pDstData, mapped.pData, dstDataSize);

	m_pDevContext->Unmap(rSrcResource.pResource, 0);
}

RdrBuffer RdrContextD3D11::CreateVertexBuffer(const void* vertices, int size, RdrResourceUsage eUsage)
{
	RdrBuffer buffer;
	buffer.pBuffer = createBuffer(m_pDevice, vertices, size, D3D11_BIND_VERTEX_BUFFER);
	return buffer;
}

RdrBuffer RdrContextD3D11::CreateIndexBuffer(const void* indices, int size, RdrResourceUsage eUsage)
{
	RdrBuffer buffer;
	buffer.pBuffer = createBuffer(m_pDevice, indices, size, D3D11_BIND_INDEX_BUFFER);
	return buffer;
}

void RdrContextD3D11::ReleaseBuffer(const RdrBuffer& rBuffer)
{
	rBuffer.pBuffer->Release();
}

static D3D11_COMPARISON_FUNC getComparisonFuncD3d(const RdrComparisonFunc cmpFunc)
{
	D3D11_COMPARISON_FUNC cmpFuncD3d[] = {
		D3D11_COMPARISON_NEVER,
		D3D11_COMPARISON_LESS,
		D3D11_COMPARISON_EQUAL,
		D3D11_COMPARISON_LESS_EQUAL,
		D3D11_COMPARISON_GREATER,
		D3D11_COMPARISON_NOT_EQUAL,
		D3D11_COMPARISON_GREATER_EQUAL,
		D3D11_COMPARISON_ALWAYS
	};
	static_assert(ARRAY_SIZE(cmpFuncD3d) == (int)RdrComparisonFunc::Count, "Missing comparison func");
	return cmpFuncD3d[(int)cmpFunc];
}

static D3D11_TEXTURE_ADDRESS_MODE getTexCoordModeD3d(const RdrTexCoordMode uvMode)
{
	D3D11_TEXTURE_ADDRESS_MODE uvModeD3d[] = {
		D3D11_TEXTURE_ADDRESS_WRAP,
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_TEXTURE_ADDRESS_MIRROR
	};
	static_assert(ARRAY_SIZE(uvModeD3d) == (int)RdrTexCoordMode::Count, "Missing tex coord mode");
	return uvModeD3d[(int)uvMode];
}

static D3D11_FILTER getFilterD3d(const RdrComparisonFunc cmpFunc, const bool bPointSample)
{
	if (cmpFunc != RdrComparisonFunc::Never)
	{
		return bPointSample ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	}
	else
	{
		return bPointSample ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	}
}

RdrContextD3D11::RdrContextD3D11(RdrProfiler& rProfiler)
	: m_rProfiler(rProfiler)
{

}

bool RdrContextD3D11::Init(HWND hWnd, uint width, uint height)
{
	m_slopeScaledDepthBias = 3.f;

	HRESULT hr;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
	swapChainDesc.BufferCount = kNumBackBuffers;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		g_userConfig.debugDevice ? D3D11_CREATE_DEVICE_DEBUG : 0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&m_pSwapChain,
		&m_pDevice,
		nullptr,
		&m_pDevContext);

	if (hr != S_OK)
	{
		MessageBox(NULL, L"Failed to create D3D11 device", L"Failure", MB_OK);
		return false;
	}

	// Annotator
	m_pDevContext->QueryInterface(__uuidof(m_pAnnotator), (void**)&m_pAnnotator);

	// Debug settings
	if (g_userConfig.debugDevice)
	{
		if (SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&m_pDebug)))
		{
			if (SUCCEEDED(m_pDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&m_pInfoQueue)))
			{
				m_pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
				m_pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
			}
		}
	}

	Resize(width, height);
	ResetDrawState();
	return true;
}

void RdrContextD3D11::Release()
{
	m_pSwapChain->Release();
	m_pPrimaryRenderTarget->Release();

	m_pDevContext->Release();
	m_pDevice->Release();
}

bool RdrContextD3D11::IsIdle()
{
	return (m_presentFlags & DXGI_PRESENT_TEST);
}

ID3D11SamplerState* RdrContextD3D11::GetSampler(const RdrSamplerState& state)
{
	uint samplerIndex =
		state.cmpFunc * ((uint)RdrTexCoordMode::Count * 2)
		+ state.texcoordMode * 2
		+ state.bPointSample;

	if (!m_pSamplers[samplerIndex])
	{
		D3D11_SAMPLER_DESC desc;

		desc.MaxAnisotropy = 1;
		desc.MaxLOD = FLT_MAX;
		desc.MinLOD = -FLT_MIN;
		desc.MipLODBias = 0;
		desc.BorderColor[0] = 1.f;
		desc.BorderColor[1] = 1.f;
		desc.BorderColor[2] = 1.f;
		desc.BorderColor[3] = 1.f;

		desc.AddressU = desc.AddressV = desc.AddressW = getTexCoordModeD3d((RdrTexCoordMode)state.texcoordMode);
		desc.Filter = getFilterD3d((RdrComparisonFunc)state.cmpFunc, state.bPointSample);
		desc.ComparisonFunc = getComparisonFuncD3d((RdrComparisonFunc)state.cmpFunc);

		HRESULT hr = m_pDevice->CreateSamplerState(&desc, &m_pSamplers[samplerIndex]);
		assert(hr == S_OK);
	}

	return m_pSamplers[samplerIndex];
}

namespace
{
	bool createTextureCubeSrv(ID3D11Device* pDevice, const RdrTextureInfo& rTexInfo, const RdrResource& rResource, ID3D11ShaderResourceView** ppOutView)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(rTexInfo.format);

		if (rTexInfo.depth > 1)
		{
			viewDesc.TextureCubeArray.First2DArrayFace = 0;
			viewDesc.TextureCubeArray.MipLevels = rTexInfo.mipLevels;
			viewDesc.TextureCubeArray.MostDetailedMip = 0;
			viewDesc.TextureCubeArray.NumCubes = rTexInfo.depth;
			viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
		}
		else
		{
			viewDesc.TextureCube.MipLevels = rTexInfo.mipLevels;
			viewDesc.TextureCube.MostDetailedMip = 0;
			viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		}

		HRESULT hr = pDevice->CreateShaderResourceView(rResource.pTexture2d, &viewDesc, ppOutView);
		assert(hr == S_OK);
		return true;
	}

	bool createTextureCube(ID3D11Device* pDevice, D3D11_SUBRESOURCE_DATA* pData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResource& rResource)
	{
		const uint cubemapArraySize = rTexInfo.depth * 6;
		D3D11_TEXTURE2D_DESC desc = { 0 };

		desc.Usage = getD3DUsage(eUsage);
		desc.CPUAccessFlags = (desc.Usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE : 0;
		desc.ArraySize = cubemapArraySize;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if (resourceFormatIsDepth(rTexInfo.format))
			desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
		else if (eUsage != RdrResourceUsage::Immutable)
			desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		desc.SampleDesc.Count = rTexInfo.sampleCount;
		desc.Format = getD3DTypelessFormat(rTexInfo.format);
		desc.Width = rTexInfo.width;
		desc.Height = rTexInfo.height;
		desc.MipLevels = rTexInfo.mipLevels;
		desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

		HRESULT hr = pDevice->CreateTexture2D(&desc, pData, &rResource.pTexture2d);
		assert(hr == S_OK);

		//if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
		{
			createTextureCubeSrv(pDevice, rTexInfo, rResource, &rResource.resourceView.pViewD3D11);
		}

		return true;
	}

	bool createTexture2DSrv(ID3D11Device* pDevice, const RdrTextureInfo& rTexInfo, const RdrResource& rResource, ID3D11ShaderResourceView** ppOutView)
	{
		bool bIsMultisampled = (rTexInfo.sampleCount > 1);
		bool bIsArray = (rTexInfo.depth > 1);

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(rTexInfo.format);
		if (bIsArray)
		{
			if (bIsMultisampled)
			{
				viewDesc.Texture2DMSArray.ArraySize = rTexInfo.depth;
				viewDesc.Texture2DMSArray.FirstArraySlice = 0;
				viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
			}
			else
			{
				viewDesc.Texture2DArray.ArraySize = rTexInfo.depth;
				viewDesc.Texture2DArray.FirstArraySlice = 0;
				viewDesc.Texture2DArray.MipLevels = rTexInfo.mipLevels;
				viewDesc.Texture2DArray.MostDetailedMip = 0;
				viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			}
		}
		else
		{
			if (bIsMultisampled)
			{
				viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				viewDesc.Texture2D.MipLevels = rTexInfo.mipLevels;
				viewDesc.Texture2D.MostDetailedMip = 0;
				viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			}
		}

		HRESULT hr = pDevice->CreateShaderResourceView(rResource.pTexture2d, &viewDesc, ppOutView);
		assert(hr == S_OK);

		return true;
	}

	bool createTexture2D(ID3D11Device* pDevice, D3D11_SUBRESOURCE_DATA* pData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResourceBindings eBindings, RdrResource& rResource)
	{
		bool bIsMultisampled = (rTexInfo.sampleCount > 1);
		bool bIsArray = (rTexInfo.depth > 1);
		
		D3D11_TEXTURE2D_DESC desc = { 0 };

		desc.Usage = getD3DUsage(eUsage);
		desc.ArraySize = rTexInfo.depth;
		desc.MiscFlags = 0;
		if (desc.Usage == D3D11_USAGE_STAGING)
		{
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		}
		else
		{
			if (desc.Usage == D3D11_USAGE_DYNAMIC)
			{
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			}

			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			if (resourceFormatIsDepth(rTexInfo.format))
			{
				desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
			}
			else
			{
				if (eBindings == RdrResourceBindings::kUnorderedAccessView)
				{
					desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
				}
				else if (eBindings == RdrResourceBindings::kRenderTarget)
				{
					desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
				}
			}
		}

		desc.SampleDesc.Count = rTexInfo.sampleCount;
		desc.SampleDesc.Quality = 0;
		desc.Format = getD3DTypelessFormat(rTexInfo.format);
		desc.Width = rTexInfo.width;
		desc.Height = rTexInfo.height;
		desc.MipLevels = rTexInfo.mipLevels;

		HRESULT hr = pDevice->CreateTexture2D(&desc, pData, &rResource.pTexture2d);
		assert(hr == S_OK);

		if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
		{
			createTexture2DSrv(pDevice, rTexInfo, rResource, &rResource.resourceView.pViewD3D11);
		}

		if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc;
			viewDesc.Format = getD3DFormat(rTexInfo.format);
			if (bIsArray)
			{
				viewDesc.Texture2DArray.ArraySize = rTexInfo.depth;
				viewDesc.Texture2DArray.FirstArraySlice = 0;
				viewDesc.Texture2DArray.MipSlice = 0;
				viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			}
			else
			{
				viewDesc.Texture2D.MipSlice = 0;
				viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			}

			hr = pDevice->CreateUnorderedAccessView(rResource.pTexture2d, &viewDesc, &rResource.uav.pViewD3D11);
			assert(hr == S_OK);
		}

		return true;
	}

	bool createTexture3DSrv(ID3D11Device* pDevice, const RdrTextureInfo& rTexInfo, const RdrResource& rResource, ID3D11ShaderResourceView** ppOutView)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(rTexInfo.format);
		viewDesc.Texture3D.MipLevels = rTexInfo.mipLevels;
		viewDesc.Texture3D.MostDetailedMip = 0;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;

		HRESULT hr = pDevice->CreateShaderResourceView(rResource.pTexture3d, &viewDesc, ppOutView);
		assert(hr == S_OK);

		return true;
	}

	bool createTexture3D(ID3D11Device* pDevice, D3D11_SUBRESOURCE_DATA* pData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResource& rResource)
	{
		bool bCanBindAccessView = !resourceFormatIsCompressed(rTexInfo.format) && eUsage != RdrResourceUsage::Immutable;

		D3D11_TEXTURE3D_DESC desc = { 0 };

		desc.Usage = getD3DUsage(eUsage);
		desc.MiscFlags = 0;
		if (desc.Usage == D3D11_USAGE_STAGING)
		{
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		}
		else
		{
			if (desc.Usage == D3D11_USAGE_DYNAMIC)
			{
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			}
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			if (resourceFormatIsDepth(rTexInfo.format))
			{
				desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
			}
			else
			{
				// todo: find out if having all these bindings cause any inefficiencies in D3D
				if (bCanBindAccessView)
					desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
				if (eUsage != RdrResourceUsage::Immutable)
					desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			}
		}

		desc.Format = getD3DTypelessFormat(rTexInfo.format);
		desc.Width = rTexInfo.width;
		desc.Height = rTexInfo.height;
		desc.Depth = rTexInfo.depth;
		desc.MipLevels = rTexInfo.mipLevels;

		HRESULT hr = pDevice->CreateTexture3D(&desc, pData, &rResource.pTexture3d);
		assert(hr == S_OK);

		if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
		{
			createTexture3DSrv(pDevice, rTexInfo, rResource, &rResource.resourceView.pViewD3D11);
		}

		if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc;
			viewDesc.Format = getD3DFormat(rTexInfo.format);
			viewDesc.Texture3D.MipSlice = 0;
			viewDesc.Texture3D.FirstWSlice = 0;
			viewDesc.Texture3D.WSize = rTexInfo.depth;
			viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;

			hr = pDevice->CreateUnorderedAccessView(rResource.pTexture3d, &viewDesc, &rResource.uav.pViewD3D11);
			assert(hr == S_OK);
		}

		return true;
	}
}

bool RdrContextD3D11::CreateTexture(const void* pSrcData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResourceBindings eBindings, RdrResource& rResource)
{
	uint sliceCount = (rTexInfo.texType == RdrTextureType::kCube) ? rTexInfo.depth * 6 : rTexInfo.depth;
	D3D11_SUBRESOURCE_DATA* pData = nullptr;  
	
	if (pSrcData)
	{
		uint resIndex = 0;
		pData = (D3D11_SUBRESOURCE_DATA*)_malloca(sliceCount * rTexInfo.mipLevels * sizeof(D3D11_SUBRESOURCE_DATA));

		char* pPos = (char*)pSrcData;
		for (uint slice = 0; slice < sliceCount; ++slice)
		{
			uint mipWidth = rTexInfo.width;
			uint mipHeight = rTexInfo.height;
			for (uint mip = 0; mip < rTexInfo.mipLevels; ++mip)
			{
				pData[resIndex].pSysMem = pPos;
				pData[resIndex].SysMemPitch = rdrGetTexturePitch(mipWidth, rTexInfo.format);

				int rows = rdrGetTextureRows(mipHeight, rTexInfo.format);
				pData[resIndex].SysMemSlicePitch = pData[resIndex].SysMemPitch * rows;
				pPos += pData[resIndex].SysMemPitch * rows;

				if (mipWidth > 1)
					mipWidth >>= 1;
				if (mipHeight > 1)
					mipHeight >>= 1;
				++resIndex;
			}
		}
	}

	bool res = false;
	switch (rTexInfo.texType)
	{
	case RdrTextureType::k2D:
		res = createTexture2D(m_pDevice, pData, rTexInfo, eUsage, eBindings, rResource);
		break;
	case RdrTextureType::kCube:
		res = createTextureCube(m_pDevice, pData, rTexInfo, eUsage, rResource);
		break;
	case RdrTextureType::k3D:
		res = createTexture3D(m_pDevice, pData, rTexInfo, eUsage, rResource);
		break;
	}

	if (pData)
		_freea(pData);
	return res;
}

void RdrContextD3D11::ReleaseResource(RdrResource& rResource)
{
	if (rResource.pResource)
	{
		rResource.pResource->Release();
		rResource.pResource = nullptr;
	}
	if (rResource.resourceView.pViewD3D11)
	{
		rResource.resourceView.pViewD3D11->Release();
		rResource.resourceView.pViewD3D11 = nullptr;
	}
	if (rResource.uav.pViewD3D11)
	{
		rResource.uav.pViewD3D11->Release();
		rResource.uav.pViewD3D11 = nullptr;
	}
}

void RdrContextD3D11::ResolveResource(const RdrResource& rSrc, const RdrResource& rDst)
{
	m_pDevContext->ResolveSubresource(rDst.pResource, 0, rSrc.pResource, 0, getD3DFormat(rSrc.texInfo.format));
}

RdrShaderResourceView RdrContextD3D11::CreateShaderResourceViewTexture(const RdrResource& rResource)
{
	RdrShaderResourceView view = { 0 };
	switch (rResource.texInfo.texType)
	{
	case RdrTextureType::k2D:
		createTexture2DSrv(m_pDevice, rResource.texInfo, rResource, &view.pViewD3D11);
		break;
	case RdrTextureType::k3D:
		createTexture3DSrv(m_pDevice, rResource.texInfo, rResource, &view.pViewD3D11);
		break;
	case RdrTextureType::kCube:
		createTextureCubeSrv(m_pDevice, rResource.texInfo, rResource, &view.pViewD3D11);
		break;
	}
	return view;
}

RdrShaderResourceView RdrContextD3D11::CreateShaderResourceViewBuffer(const RdrResource& rResource, uint firstElement)
{
	RdrShaderResourceView view = { 0 };

	const RdrBufferInfo& rBufferInfo = rResource.bufferInfo;
	createBufferSrv(m_pDevice, rResource, rBufferInfo.numElements - firstElement, rBufferInfo.eFormat, firstElement, &view.pViewD3D11);

	return view;
}

void RdrContextD3D11::ReleaseShaderResourceView(RdrShaderResourceView& resourceView)
{
	resourceView.pViewD3D11->Release();
	resourceView.pTypeless = nullptr;
}

RdrDepthStencilView RdrContextD3D11::CreateDepthStencilView(const RdrResource& rDepthTex)
{
	RdrDepthStencilView view;

	bool bIsMultisampled = (rDepthTex.texInfo.sampleCount > 1);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = getD3DDepthFormat(rDepthTex.texInfo.format);
	if (rDepthTex.texInfo.depth > 1)
	{
		if (bIsMultisampled)
		{
			dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
			dsvDesc.Texture2DMSArray.ArraySize = rDepthTex.texInfo.depth;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
		}
		else
		{
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.ArraySize = rDepthTex.texInfo.depth;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		}
	}
	else
	{
		if (bIsMultisampled)
		{
			dsvDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			dsvDesc.Texture2D.MipSlice = 0;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		}
	}

	HRESULT hr = m_pDevice->CreateDepthStencilView(rDepthTex.pTexture2d, &dsvDesc, &view.pView);
	assert(hr == S_OK);

	return view;
}

RdrDepthStencilView RdrContextD3D11::CreateDepthStencilView(const RdrResource& rDepthTex, uint arrayStartIndex, uint arraySize)
{
	RdrDepthStencilView view;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = getD3DDepthFormat(rDepthTex.texInfo.format);

	if (rDepthTex.texInfo.sampleCount > 1)
	{
		dsvDesc.Texture2DMSArray.FirstArraySlice = arrayStartIndex;
		dsvDesc.Texture2DMSArray.ArraySize = arraySize;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
	}
	else
	{
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = arrayStartIndex;
		dsvDesc.Texture2DArray.ArraySize = arraySize;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	}

	HRESULT hr = m_pDevice->CreateDepthStencilView(rDepthTex.pTexture2d, &dsvDesc, &view.pView);
	assert(hr == S_OK);

	return view;
}

void RdrContextD3D11::ReleaseDepthStencilView(const RdrDepthStencilView& depthStencilView)
{
	depthStencilView.pView->Release();
}

RdrRenderTargetView RdrContextD3D11::CreateRenderTargetView(RdrResource& rTexRes)
{
	RdrRenderTargetView view;

	D3D11_RENDER_TARGET_VIEW_DESC desc;
	desc.Format = getD3DFormat(rTexRes.texInfo.format);
	if (rTexRes.texInfo.sampleCount > 1)
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
		desc.Texture2DMS.UnusedField_NothingToDefine = 0;
	}
	else
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;
	}

	HRESULT hr = m_pDevice->CreateRenderTargetView(rTexRes.pTexture2d, &desc, &view.pView);
	assert(hr == S_OK);

	return view;
}

RdrRenderTargetView RdrContextD3D11::CreateRenderTargetView(RdrResource& rTexArrayRes, uint arrayStartIndex, uint arraySize)
{
	RdrRenderTargetView view;

	D3D11_RENDER_TARGET_VIEW_DESC desc;
	desc.Format = getD3DFormat(rTexArrayRes.texInfo.format);
	desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	desc.Texture2DArray.MipSlice = 0;
	desc.Texture2DArray.FirstArraySlice = arrayStartIndex;
	desc.Texture2DArray.ArraySize = arraySize;

	HRESULT hr = m_pDevice->CreateRenderTargetView(rTexArrayRes.pTexture2d, &desc, &view.pView);
	assert(hr == S_OK);

	return view;
}

void RdrContextD3D11::ReleaseRenderTargetView(const RdrRenderTargetView& renderTargetView)
{
	renderTargetView.pView->Release();
}

void RdrContextD3D11::SetDepthStencilState(RdrDepthTestMode eDepthTest, bool bWriteEnabled)
{
	if (!m_pDepthStencilStates[(int)eDepthTest * 2 + bWriteEnabled])
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc;

		dsDesc.DepthEnable = (eDepthTest != RdrDepthTestMode::None);
		switch (eDepthTest)
		{
		case RdrDepthTestMode::Less:
			dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
			break;
		case RdrDepthTestMode::Equal:
			dsDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
			break;
		default:
			dsDesc.DepthFunc = D3D11_COMPARISON_NEVER;
			break;
		}
		dsDesc.DepthWriteMask = bWriteEnabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;

		dsDesc.StencilEnable = false;

		dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		ID3D11DepthStencilState* pState;
		HRESULT hr = m_pDevice->CreateDepthStencilState(&dsDesc, &pState);
		assert(hr == S_OK);

		m_pDepthStencilStates[(int)eDepthTest] = pState;
	}

	m_pDevContext->OMSetDepthStencilState(m_pDepthStencilStates[(int)eDepthTest], 1);
}

void RdrContextD3D11::SetBlendState(RdrBlendMode blendMode)
{
	if (!m_pBlendStates[(int)blendMode])
	{
		D3D11_BLEND_DESC desc = { 0 };
		desc.AlphaToCoverageEnable = false;
		desc.IndependentBlendEnable = false;

		switch (blendMode)
		{
		case RdrBlendMode::kOpaque:
			desc.RenderTarget[0].BlendEnable = false;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
			desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			break;
		case RdrBlendMode::kAlpha:
			desc.RenderTarget[0].BlendEnable = true;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			break;
		case RdrBlendMode::kAdditive:
			desc.RenderTarget[0].BlendEnable = true;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
			desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			break;
		case RdrBlendMode::kSubtractive:
			desc.RenderTarget[0].BlendEnable = true;
			desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
			desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
			desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_REV_SUBTRACT;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			break;
		}

		ID3D11BlendState* pBlendState = nullptr;
		HRESULT hr = m_pDevice->CreateBlendState(&desc, &pBlendState);
		assert(hr == S_OK);

		m_pBlendStates[(int)blendMode] = pBlendState;
	}

	m_pDevContext->OMSetBlendState(m_pBlendStates[(int)blendMode], nullptr, 0xffffffff);
}

void RdrContextD3D11::SetRasterState(const RdrRasterState& rasterState)
{
	uint rasterIndex =
		rasterState.bEnableMSAA * 2 * 2 * 2 * 2
		+ rasterState.bEnableScissor * 2 * 2 * 2
		+ rasterState.bWireframe * 2 * 2
		+ rasterState.bUseSlopeScaledDepthBias * 2
		+ rasterState.bDoubleSided;

	if (!m_pRasterStates[rasterIndex])
	{
		D3D11_RASTERIZER_DESC desc;
		desc.AntialiasedLineEnable = !rasterState.bEnableMSAA;
		desc.MultisampleEnable = rasterState.bEnableMSAA;
		desc.CullMode = rasterState.bDoubleSided ? D3D11_CULL_NONE : D3D11_CULL_BACK;
		desc.SlopeScaledDepthBias = rasterState.bUseSlopeScaledDepthBias ? m_slopeScaledDepthBias : 0.f;
		desc.DepthBiasClamp = 0.f;
		desc.DepthClipEnable = true;
		desc.FrontCounterClockwise = false;
		desc.ScissorEnable = rasterState.bEnableScissor;
		if (rasterState.bWireframe)
		{
			desc.FillMode = D3D11_FILL_WIREFRAME;
			desc.DepthBias = -2; // Wireframe gets a slight depth bias so z-fighting doesn't occur.
		}
		else
		{
			desc.FillMode = D3D11_FILL_SOLID;
			desc.DepthBias = 0;
		}

		ID3D11RasterizerState* pRasterState = nullptr;
		HRESULT hr = m_pDevice->CreateRasterizerState(&desc, &pRasterState);
		assert(hr == S_OK);

		m_pRasterStates[rasterIndex] = pRasterState;
	}

	m_pDevContext->RSSetState(m_pRasterStates[rasterIndex]);
}

void RdrContextD3D11::SetRenderTargets(uint numTargets, const RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget)
{
	static_assert(sizeof(RdrRenderTargetView) == sizeof(ID3D11RenderTargetView*), "RdrRenderTargetView must only contain the device object pointer");
	m_pDevContext->OMSetRenderTargets(numTargets, (ID3D11RenderTargetView**)aRenderTargets, depthStencilTarget.pView);
}

void RdrContextD3D11::BeginEvent(LPCWSTR eventName)
{
	m_pAnnotator->BeginEvent(eventName);
}

void RdrContextD3D11::EndEvent()
{
	m_pAnnotator->EndEvent();
}

void RdrContextD3D11::Present()
{
	HRESULT hr = m_pSwapChain->Present(g_userConfig.vsync, m_presentFlags);
	if (hr == DXGI_STATUS_OCCLUDED)
	{
		// Window is no longer visible, most likely minimized.  
		// Set test flag so that we don't waste time presenting until the window is visible again.
		m_presentFlags |= DXGI_PRESENT_TEST;
	}
	else if (hr == S_OK)
	{
		m_presentFlags &= ~DXGI_PRESENT_TEST;
	}
	else
	{
		HRESULT hrRemovedReason = m_pDevice->GetDeviceRemovedReason();
		switch (hrRemovedReason)
		{
		case DXGI_ERROR_DEVICE_HUNG:
			Error("D3D11 device hung!");
			break;
		case DXGI_ERROR_DEVICE_REMOVED:
			Error("D3D11 device removed!");
			break;
		case DXGI_ERROR_DEVICE_RESET:
			Error("D3D11 device reset!");
			break;
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			Error("D3D11 device internal driver error!");
			break;
		case DXGI_ERROR_INVALID_CALL:
			Error("D3D11 device invalid call!");
			break;
		default:
			Error("Unknown D3D11 device error %d!", hrRemovedReason);
			break;
		}
	}

	ResetDrawState();
}

void RdrContextD3D11::Resize(uint width, uint height)
{
	if (m_pPrimaryRenderTarget)
		m_pPrimaryRenderTarget->Release();
	m_pSwapChain->ResizeBuffers(kNumBackBuffers, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

	ID3D11Texture2D* pBackBuffer;
	m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);

	D3D11_TEXTURE2D_DESC bbDesc;
	pBackBuffer->GetDesc(&bbDesc);

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = bbDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	HRESULT hr = m_pDevice->CreateRenderTargetView(pBackBuffer, &rtvDesc, &m_pPrimaryRenderTarget);
	assert(hr == S_OK);
	pBackBuffer->Release();
}

void RdrContextD3D11::ClearRenderTargetView(const RdrRenderTargetView& renderTarget, const Color& clearColor)
{
	m_pDevContext->ClearRenderTargetView(renderTarget.pView, clearColor.asFloat4());
}

void RdrContextD3D11::ClearDepthStencilView(const RdrDepthStencilView& depthStencil, const bool bClearDepth, const float depthVal, const bool bClearStencil, const uint8 stencilVal)
{
	uint clearFlags = 0;
	if (bClearDepth)
		clearFlags |= D3D11_CLEAR_DEPTH;
	if (bClearStencil)
		clearFlags |= D3D11_CLEAR_STENCIL;
	m_pDevContext->ClearDepthStencilView(depthStencil.pView, clearFlags, depthVal, stencilVal);
}

RdrRenderTargetView RdrContextD3D11::GetPrimaryRenderTarget()
{
	RdrRenderTargetView view;
	view.pView = m_pPrimaryRenderTarget;
	return view;
}

void RdrContextD3D11::SetViewport(const Rect& viewport)
{
	D3D11_VIEWPORT d3dViewport;
	d3dViewport.TopLeftX = viewport.left;
	d3dViewport.TopLeftY = viewport.top;
	d3dViewport.Width = viewport.width;
	d3dViewport.Height = viewport.height;
	d3dViewport.MinDepth = 0.f;
	d3dViewport.MaxDepth = 1.f;
	m_pDevContext->RSSetViewports(1, &d3dViewport);
}

RdrConstantBufferDeviceObj RdrContextD3D11::CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage)
{
	D3D11_BUFFER_DESC desc = { 0 };
	desc.ByteWidth = size;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = getD3DCpuAccessFlags(cpuAccessFlags);
	desc.Usage = getD3DUsage(eUsage);

	D3D11_SUBRESOURCE_DATA data = { 0 };
	data.pSysMem = pData;

	ID3D11Buffer* pBuffer = nullptr;
	HRESULT hr = m_pDevice->CreateBuffer(&desc, (pData ? &data : nullptr), &pBuffer);
	assert(hr == S_OK);

	RdrConstantBufferDeviceObj devObj;
	devObj.pBufferD3D11 = pBuffer;
	return devObj;
}

void RdrContextD3D11::UpdateConstantBuffer(const RdrConstantBufferDeviceObj& buffer, const void* pSrcData, const uint dataSize)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = m_pDevContext->Map(buffer.pBufferD3D11, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	memcpy(mapped.pData, pSrcData, dataSize);

	m_pDevContext->Unmap(buffer.pBufferD3D11, 0);
}

void RdrContextD3D11::ReleaseConstantBuffer(const RdrConstantBufferDeviceObj& buffer)
{
	buffer.pBufferD3D11->Release();
}

// Compares array data between src and dst, copying differences to dst and returning the range of changed elements.
template<typename DataT>
static inline bool updateDataList(const DataT* aSrcData, DataT* aDstData, uint count, int* pOutFirstChanged, int* pOutLastChanged)
{
	int firstChanged = -1;
	int lastChanged = -1;
	for (uint i = 0; i < count; ++i)
	{
		if (aSrcData[i] != aDstData[i])
		{
			aDstData[i] = aSrcData[i];
			lastChanged = i;
			if (firstChanged < 0)
				firstChanged = i;
		}
	}

	*pOutFirstChanged = firstChanged;
	*pOutLastChanged = lastChanged;
	return (firstChanged >= 0);
}

void RdrContextD3D11::Draw(const RdrDrawState& rDrawState, uint instanceCount)
{
	static_assert(sizeof(RdrBuffer) == sizeof(ID3D11Buffer*), "RdrBuffer must only contain the device obj pointer.");
	static_assert(sizeof(RdrConstantBufferDeviceObj) == sizeof(ID3D11Buffer*), "RdrConstantBufferDeviceObj must only contain the device obj pointer.");
	static_assert(sizeof(RdrShaderResourceView) == sizeof(ID3D11ShaderResourceView*), "RdrVertexBuffer must only contain the device obj pointer.");
	
	int firstChanged = -1;
	int lastChanged = -1;

	//////////////////////////////////////////////////////////////////////////
	// Input assembler

	// Primitive topology
	if (rDrawState.eTopology != m_drawState.eTopology)
	{
		m_pDevContext->IASetPrimitiveTopology(getD3DTopology(rDrawState.eTopology));
		m_drawState.eTopology = rDrawState.eTopology;
		m_rProfiler.IncrementCounter(RdrProfileCounter::PrimitiveTopology);
	}

	// Input layout
	if (rDrawState.inputLayout.pInputLayout != m_drawState.inputLayout.pInputLayout)
	{
		m_pDevContext->IASetInputLayout(rDrawState.inputLayout.pInputLayout);
		m_drawState.inputLayout.pInputLayout = rDrawState.inputLayout.pInputLayout;
		m_rProfiler.IncrementCounter(RdrProfileCounter::InputLayout);
	}

	// Vertex buffers
	if (updateDataList<RdrBuffer>(rDrawState.vertexBuffers, m_drawState.vertexBuffers,
		rDrawState.vertexBufferCount, &firstChanged, &lastChanged))
	{
		m_pDevContext->IASetVertexBuffers(firstChanged, lastChanged - firstChanged + 1, 
			(ID3D11Buffer**)rDrawState.vertexBuffers + firstChanged, rDrawState.vertexStrides + firstChanged, rDrawState.vertexOffsets + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::VertexBuffer);
	}

	//////////////////////////////////////////////////////////////////////////
	// Vertex shader
	if (rDrawState.pVertexShader != m_drawState.pVertexShader)
	{
		m_pDevContext->VSSetShader(rDrawState.pVertexShader->pVertex, nullptr, 0);
		m_drawState.pVertexShader = rDrawState.pVertexShader;
		m_rProfiler.IncrementCounter(RdrProfileCounter::VertexShader);
	}

	// VS constants
	if (updateDataList<RdrConstantBufferDeviceObj>(rDrawState.vsConstantBuffers, m_drawState.vsConstantBuffers,
		rDrawState.vsConstantBufferCount, &firstChanged, &lastChanged))
	{
		m_pDevContext->VSSetConstantBuffers(firstChanged, lastChanged - firstChanged + 1, (ID3D11Buffer**)rDrawState.vsConstantBuffers + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::VsConstantBuffer);
	}

	// VS resources
	if (updateDataList<RdrShaderResourceView>(rDrawState.vsResources, m_drawState.vsResources,
		rDrawState.vsResourceCount, &firstChanged, &lastChanged))
	{
		m_pDevContext->VSSetShaderResources(firstChanged, lastChanged - firstChanged + 1, (ID3D11ShaderResourceView**)rDrawState.vsResources + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::VsResource);
	}

	//////////////////////////////////////////////////////////////////////////
	// Geometry shader
	if (rDrawState.pGeometryShader != m_drawState.pGeometryShader)
	{
		if (rDrawState.pGeometryShader)
		{
			m_pDevContext->GSSetShader(rDrawState.pGeometryShader->pGeometry, nullptr, 0);
		}
		else
		{
			m_pDevContext->GSSetShader(nullptr, nullptr, 0);
		}
		m_rProfiler.IncrementCounter(RdrProfileCounter::GeometryShader);
		m_drawState.pGeometryShader = rDrawState.pGeometryShader;
	}

	// GS constants
	if (updateDataList<RdrConstantBufferDeviceObj>(rDrawState.gsConstantBuffers, m_drawState.gsConstantBuffers,
		rDrawState.gsConstantBufferCount, &firstChanged, &lastChanged))
	{
		m_pDevContext->GSSetConstantBuffers(firstChanged, lastChanged - firstChanged + 1, (ID3D11Buffer**)rDrawState.gsConstantBuffers + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::GsConstantBuffer);
	}

	//////////////////////////////////////////////////////////////////////////
	// Hull shader
	if (rDrawState.pHullShader != m_drawState.pHullShader)
	{
		if (rDrawState.pHullShader)
		{
			m_pDevContext->HSSetShader(rDrawState.pHullShader->pHull, nullptr, 0);
		}
		else
		{
			m_pDevContext->HSSetShader(nullptr, nullptr, 0);
		}
		m_drawState.pHullShader = rDrawState.pHullShader;
		m_rProfiler.IncrementCounter(RdrProfileCounter::HullShader);
	}

	//////////////////////////////////////////////////////////////////////////
	// Domain shader
	if (rDrawState.pDomainShader != m_drawState.pDomainShader)
	{
		if (rDrawState.pDomainShader)
		{
			m_pDevContext->DSSetShader(rDrawState.pDomainShader->pDomain, nullptr, 0);
		}
		else
		{
			m_pDevContext->DSSetShader(nullptr, nullptr, 0);
		}
		m_drawState.pDomainShader = rDrawState.pDomainShader;
		m_rProfiler.IncrementCounter(RdrProfileCounter::DomainShader);
	}

	if (rDrawState.pDomainShader)
	{
		if (updateDataList<RdrConstantBufferDeviceObj>(rDrawState.dsConstantBuffers, m_drawState.dsConstantBuffers,
			rDrawState.dsConstantBufferCount, &firstChanged, &lastChanged))
		{
			m_pDevContext->DSSetConstantBuffers(firstChanged, lastChanged - firstChanged + 1, (ID3D11Buffer**)rDrawState.dsConstantBuffers + firstChanged);
			m_rProfiler.IncrementCounter(RdrProfileCounter::DsConstantBuffer);
		}

		if (updateDataList<RdrShaderResourceView>(rDrawState.dsResources, m_drawState.dsResources,
			rDrawState.dsResourceCount, &firstChanged, &lastChanged))
		{
			m_pDevContext->DSSetShaderResources(firstChanged, lastChanged - firstChanged + 1, (ID3D11ShaderResourceView**)rDrawState.dsResources + firstChanged);
			m_rProfiler.IncrementCounter(RdrProfileCounter::DsResource);
		}

		if (updateDataList<RdrSamplerState>(rDrawState.dsSamplers, m_drawState.dsSamplers,
			rDrawState.dsSamplerCount, &firstChanged, &lastChanged))
		{
			ID3D11SamplerState* apSamplers[ARRAY_SIZE(rDrawState.dsSamplers)] = { 0 };
			for (int i = firstChanged; i <= lastChanged; ++i)
			{
				apSamplers[i] = GetSampler(rDrawState.dsSamplers[i]);
			}
			m_pDevContext->DSSetSamplers(firstChanged, lastChanged - firstChanged + 1, apSamplers + firstChanged);
			m_rProfiler.IncrementCounter(RdrProfileCounter::DsSamplers);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Pixel shader
	if (rDrawState.pPixelShader != m_drawState.pPixelShader)
	{
		if (rDrawState.pPixelShader)
		{
			m_pDevContext->PSSetShader(rDrawState.pPixelShader->pPixel, nullptr, 0);
		}
		else
		{
			m_pDevContext->PSSetShader(nullptr, nullptr, 0);
		}
		m_drawState.pPixelShader = rDrawState.pPixelShader;
		m_rProfiler.IncrementCounter(RdrProfileCounter::PixelShader);
	}

	// PS constants
	if (updateDataList<RdrConstantBufferDeviceObj>(rDrawState.psConstantBuffers, m_drawState.psConstantBuffers,
		rDrawState.psConstantBufferCount, &firstChanged, &lastChanged))
	{
		m_pDevContext->PSSetConstantBuffers(firstChanged, lastChanged - firstChanged + 1, (ID3D11Buffer**)rDrawState.psConstantBuffers + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsConstantBuffer);
	}

	// PS resources
	if (updateDataList<RdrShaderResourceView>(rDrawState.psResources, m_drawState.psResources,
		rDrawState.psResourceCount, &firstChanged, &lastChanged))
	{
		m_pDevContext->PSSetShaderResources(firstChanged, lastChanged - firstChanged + 1, (ID3D11ShaderResourceView**)rDrawState.psResources + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsResource);
	}

	// PS samplers
	if (updateDataList<RdrSamplerState>(rDrawState.psSamplers, m_drawState.psSamplers,
		rDrawState.psSamplerCount, &firstChanged, &lastChanged))
	{
		ID3D11SamplerState* apSamplers[ARRAY_SIZE(rDrawState.psSamplers)] = { 0 };
		for (int i = firstChanged; i <= lastChanged; ++i)
		{
			apSamplers[i] = GetSampler(rDrawState.psSamplers[i]);
		}
		m_pDevContext->PSSetSamplers(firstChanged, lastChanged - firstChanged + 1, apSamplers + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsSamplers);
	}

	if (rDrawState.indexBuffer.pBuffer)
	{
		if (rDrawState.indexBuffer.pBuffer != m_drawState.indexBuffer.pBuffer)
		{
			m_pDevContext->IASetIndexBuffer(rDrawState.indexBuffer.pBuffer, DXGI_FORMAT_R16_UINT, 0);
			m_drawState.indexBuffer.pBuffer = rDrawState.indexBuffer.pBuffer;
			m_rProfiler.IncrementCounter(RdrProfileCounter::IndexBuffer);
		}

		if (instanceCount > 1)
		{
			m_pDevContext->DrawIndexedInstanced(rDrawState.indexCount, instanceCount, 0, 0, 0);
		}
		else
		{
			m_pDevContext->DrawIndexed(rDrawState.indexCount, 0, 0);
		}
	}
	else
	{
		if (instanceCount > 1)
		{
			m_pDevContext->DrawInstanced(rDrawState.vertexCount, instanceCount, 0, 0);
		}
		else
		{
			m_pDevContext->Draw(rDrawState.vertexCount, 0);
		}
	}
}

void RdrContextD3D11::DispatchCompute(const RdrDrawState& rDrawState, uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ)
{
	PSClearResources();
	ResetDrawState();

	m_pDevContext->CSSetShader(rDrawState.pComputeShader->pCompute, nullptr, 0);

	m_pDevContext->CSSetConstantBuffers(0, rDrawState.csConstantBufferCount, (ID3D11Buffer**)rDrawState.csConstantBuffers);

	uint numResources = ARRAY_SIZE(rDrawState.csResources);
	for (uint i = 0; i < numResources; ++i)
	{
		ID3D11ShaderResourceView* pResource = rDrawState.csResources[i].pViewD3D11;
		m_pDevContext->CSSetShaderResources(i, 1, &pResource);
	}

	uint numSamplers = ARRAY_SIZE(rDrawState.csSamplers);
	for (uint i = 0; i < numSamplers; ++i)
	{
		ID3D11SamplerState* pSampler = GetSampler(rDrawState.csSamplers[i]);
		m_pDevContext->CSSetSamplers(i, 1, &pSampler);
	}

	uint numUavs = ARRAY_SIZE(rDrawState.csUavs);
	for (uint i = 0; i < numUavs; ++i)
	{
		ID3D11UnorderedAccessView* pView = rDrawState.csUavs[i].pViewD3D11;
		m_pDevContext->CSSetUnorderedAccessViews(i, 1, &pView, nullptr);
	}

	m_pDevContext->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);

	// Clear resources to avoid binding errors (input bound as output).  todo: don't do this
	for (uint i = 0; i < numResources; ++i)
	{
		ID3D11ShaderResourceView* pResourceView = nullptr;
		m_pDevContext->CSSetShaderResources(i, 1, &pResourceView);
	}
	for (uint i = 0; i < numUavs; ++i)
	{
		ID3D11UnorderedAccessView* pUnorderedAccessView = nullptr;
		m_pDevContext->CSSetUnorderedAccessViews(i, 1, &pUnorderedAccessView, nullptr);
	}

	ResetDrawState();
}

void RdrContextD3D11::PSClearResources()
{
	memset(m_drawState.psResources, 0, sizeof(m_drawState.psResources));

	ID3D11ShaderResourceView* resourceViews[ARRAY_SIZE(RdrDrawState::psResources)] = { 0 };
	m_pDevContext->PSSetShaderResources(0, 20, resourceViews);
}

RdrQuery RdrContextD3D11::CreateQuery(RdrQueryType eType)
{
	RdrQuery query;

	D3D11_QUERY_DESC desc;
	desc.MiscFlags = 0;

	switch (eType)
	{
	case RdrQueryType::Timestamp:
		desc.Query = D3D11_QUERY_TIMESTAMP;
		break;
	case RdrQueryType::Disjoint:
		desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		break;
	}

	m_pDevice->CreateQuery(&desc, &query.pQueryD3D11);
	return query;
}

void RdrContextD3D11::ReleaseQuery(RdrQuery& rQuery)
{
	if (rQuery.pQueryD3D11)
	{
		rQuery.pQueryD3D11->Release();
		rQuery.pQueryD3D11 = nullptr;
	}
}

void RdrContextD3D11::BeginQuery(RdrQuery& rQuery)
{
	m_pDevContext->Begin(rQuery.pQueryD3D11);
}

void RdrContextD3D11::EndQuery(RdrQuery& rQuery)
{
	m_pDevContext->End(rQuery.pQueryD3D11);
}

uint64 RdrContextD3D11::GetTimestampQueryData(RdrQuery& rQuery)
{
	uint64 timestamp;
	HRESULT hr = m_pDevContext->GetData(rQuery.pQueryD3D11, &timestamp, sizeof(timestamp), 0);
	if (hr != S_OK)
	{
		timestamp = -1;
	}
	return timestamp;
}

RdrQueryDisjointData RdrContextD3D11::GetDisjointQueryData(RdrQuery& rQuery)
{
	RdrQueryDisjointData data;

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d3dData;
	while ( m_pDevContext->GetData(rQuery.pQueryD3D11, &d3dData, sizeof(d3dData), 0) != S_OK) ;

	data.frequency = d3dData.Frequency;
	data.isDisjoint = !!d3dData.Disjoint;
	return data;
}

void RdrContextD3D11::ResetDrawState()
{
	memset(&m_drawState, -1, sizeof(m_drawState));
}