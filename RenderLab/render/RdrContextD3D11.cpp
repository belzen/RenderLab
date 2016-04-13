#include "Precompiled.h"
#include "RdrContextD3D11.h"
#include "RdrDrawOp.h"
#include "RdrDrawState.h"
#include "Light.h"

#include <d3d11.h>
#include <d3d11_1.h>
#pragma comment (lib, "d3d11.lib")

namespace
{
	static bool s_useDebugDevice = 1;

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
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,	// kRdrTopology_TriangleList
		};
		static_assert(ARRAYSIZE(s_d3dTopology) == (int)RdrTopology::Count, "Missing D3D topologies!");
		return s_d3dTopology[(int)eTopology];
	}

	DXGI_FORMAT getD3DFormat(const RdrResourceFormat format)
	{
		static const DXGI_FORMAT s_d3dFormats[] = {
			DXGI_FORMAT_R16_UNORM,				// ResourceFormat::D16
			DXGI_FORMAT_R24_UNORM_X8_TYPELESS,	// ResourceFormat::D24_UNORM_S8_UINT
			DXGI_FORMAT_R16_UNORM,				// ResourceFormat::R16_UNORM
			DXGI_FORMAT_R16G16_FLOAT,			// ResourceFormat::R16G16_FLOAT
			DXGI_FORMAT_R8_UNORM,				// ResourceFormat::R8_UNORM
			DXGI_FORMAT_BC1_UNORM,				// ResourceFormat::DXT1
			DXGI_FORMAT_BC3_UNORM,				// ResourceFormat::DXT5
			DXGI_FORMAT_BC3_UNORM_SRGB,			// ResourceFormat::DXT5_sRGB
			DXGI_FORMAT_B8G8R8A8_UNORM,			// ResourceFormat::B8G8R8A8_UNORM
			DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,	// ResourceFormat::B8G8R8A8_UNORM_sRGB
			DXGI_FORMAT_R16G16B16A16_FLOAT,		// ResourceFormat::R16G16B16A16_FLOAT
		};
		static_assert(ARRAYSIZE(s_d3dFormats) == (int)RdrResourceFormat::Count, "Missing D3D formats!");
		return s_d3dFormats[(int)format];
	}

	DXGI_FORMAT getD3DDepthFormat(const RdrResourceFormat format)
	{
		static const DXGI_FORMAT s_d3dDepthFormats[] = {
			DXGI_FORMAT_D16_UNORM,			// ResourceFormat::D16
			DXGI_FORMAT_D24_UNORM_S8_UINT,	// ResourceFormat::D24_UNORM_S8_UINT
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R16_UNORM
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R16G16_FLOAT
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R8_UNORM
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT1
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT5
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT5_sRGB
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::B8G8R8A8_UNORM
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::B8G8R8A8_UNORM_sRGB
			DXGI_FORMAT_UNKNOWN,	        // ResourceFormat::R16G16B16A16_FLOAT
		};
		static_assert(ARRAYSIZE(s_d3dDepthFormats) == (int)RdrResourceFormat::Count, "Missing D3D depth formats!");
		assert(s_d3dDepthFormats[(int)format] != DXGI_FORMAT_UNKNOWN);
		return s_d3dDepthFormats[(int)format];
	}

	DXGI_FORMAT getD3DTypelessFormat(const RdrResourceFormat format)
	{
		static const DXGI_FORMAT s_d3dTypelessFormats[] = {
			DXGI_FORMAT_R16_TYPELESS,		   // ResourceFormat::D16
			DXGI_FORMAT_R24G8_TYPELESS,		   // ResourceFormat::D24_UNORM_S8_UINT
			DXGI_FORMAT_R16_TYPELESS,		   // ResourceFormat::R16_UNORM
			DXGI_FORMAT_R16G16_TYPELESS,	   // ResourceFormat::R16G16_FLOAT
			DXGI_FORMAT_R8_TYPELESS,		   // ResourceFormat::R8_UNORM
			DXGI_FORMAT_BC1_TYPELESS,		   // ResourceFormat::DXT1
			DXGI_FORMAT_BC3_TYPELESS,		   // ResourceFormat::DXT5
			DXGI_FORMAT_BC3_TYPELESS,		   // ResourceFormat::DXT5_sRGB
			DXGI_FORMAT_B8G8R8A8_TYPELESS,	   // ResourceFormat::B8G8R8A8_UNORM
			DXGI_FORMAT_B8G8R8A8_TYPELESS,     // ResourceFormat::B8G8R8A8_UNORM_sRGB
			DXGI_FORMAT_R16G16B16A16_TYPELESS, // ResourceFormat::R16G16B16A16_FLOAT
		};
		static_assert(ARRAYSIZE(s_d3dTypelessFormats) == (int)RdrResourceFormat::Count, "Missing typeless formats!");
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
		static_assert(ARRAYSIZE(s_d3dUsage) == (int)RdrResourceUsage::Count, "Missing D3D11 resource usage!");
		return s_d3dUsage[(int)eUsage];
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

bool RdrContextD3D11::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage, RdrResource& rResource)
{
	D3D11_BUFFER_DESC desc = { 0 };
	D3D11_SUBRESOURCE_DATA data = { 0 };

	desc.Usage = getD3DUsage(eUsage);
	desc.ByteWidth = numElements * elementSize;
	desc.StructureByteStride = elementSize;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	if (eUsage == RdrResourceUsage::Dynamic)
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
	hr = m_pDevice->CreateShaderResourceView(rResource.pResource, nullptr, &rResource.resourceView.pViewD3D11);
	assert(hr == S_OK);
	if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		hr = m_pDevice->CreateUnorderedAccessView(rResource.pResource, nullptr, &rResource.uav.pViewD3D11);
		assert(hr == S_OK);
	}

	return true;
}

bool RdrContextD3D11::UpdateStructuredBuffer(const void* pSrcData, RdrResource& rResource)
{
	D3D11_MAPPED_SUBRESOURCE mapped = { 0 };
	HRESULT hr = m_pDevContext->Map(rResource.pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	assert(hr == S_OK);

	memcpy(mapped.pData, pSrcData, rResource.bufferInfo.elementSize * rResource.bufferInfo.numElements);

	m_pDevContext->Unmap(rResource.pBuffer, 0);

	return true;
}

RdrVertexBuffer RdrContextD3D11::CreateVertexBuffer(const void* vertices, int size)
{
	RdrVertexBuffer buffer;
	buffer.pBuffer = createBuffer(m_pDevice, vertices, size, D3D11_BIND_VERTEX_BUFFER);
	return buffer;
}

void RdrContextD3D11::ReleaseVertexBuffer(const RdrVertexBuffer* pBuffer)
{
	pBuffer->pBuffer->Release();
}

RdrIndexBuffer RdrContextD3D11::CreateIndexBuffer(const void* indices, int size)
{
	RdrIndexBuffer buffer;
	buffer.pBuffer = createBuffer(m_pDevice, indices, size, D3D11_BIND_INDEX_BUFFER);
	return buffer;
}

void RdrContextD3D11::ReleaseIndexBuffer(const RdrIndexBuffer* pBuffer)
{
	pBuffer->pBuffer->Release();
}

static int GetTexturePitch(const int width, const RdrResourceFormat eFormat)
{
	switch (eFormat)
	{
	case RdrResourceFormat::R8_UNORM:
		return width * 1;
	case RdrResourceFormat::DXT1:
		return ((width + 3) & ~3) * 2;
	case RdrResourceFormat::DXT5:
	case RdrResourceFormat::DXT5_sRGB:
		return ((width + 3) & ~3) * 4;
	case RdrResourceFormat::B8G8R8A8_UNORM:
	case RdrResourceFormat::B8G8R8A8_UNORM_sRGB:
		return width * 4;
	default:
		assert(false);
		return 0;
	}
}

static int GetTextureRows(const int height, const RdrResourceFormat eFormat)
{
	switch (eFormat)
	{
	case RdrResourceFormat::R8_UNORM:
		return height;
	case RdrResourceFormat::DXT1:
	case RdrResourceFormat::DXT5:
	case RdrResourceFormat::DXT5_sRGB:
		return ((height + 3) & ~3) / 4;
	case RdrResourceFormat::B8G8R8A8_UNORM:
	case RdrResourceFormat::B8G8R8A8_UNORM_sRGB:
		return height;
	default:
		assert(false);
		return 0;
	}
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
	static_assert(ARRAYSIZE(cmpFuncD3d) == (int)RdrComparisonFunc::Count, "Missing comparison func");
	return cmpFuncD3d[(int)cmpFunc];
}

static D3D11_TEXTURE_ADDRESS_MODE getTexCoordModeD3d(const RdrTexCoordMode uvMode)
{
	D3D11_TEXTURE_ADDRESS_MODE uvModeD3d[] = {
		D3D11_TEXTURE_ADDRESS_WRAP,
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_TEXTURE_ADDRESS_MIRROR
	};
	static_assert(ARRAYSIZE(uvModeD3d) == (int)RdrTexCoordMode::Count, "Missing tex coord mode");
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

bool RdrContextD3D11::Init(HWND hWnd, uint width, uint height)
{
	HRESULT hr;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
	swapChainDesc.BufferCount = 1;
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
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		s_useDebugDevice ? D3D11_CREATE_DEVICE_DEBUG : 0,
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

	Resize(width, height);
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
	bool CreateTextureCube(ID3D11Device* pDevice, D3D11_SUBRESOURCE_DATA* pData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResource& rResource)
	{
		const uint cubemapArraySize = rTexInfo.arraySize * 6;
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

		HRESULT hr = pDevice->CreateTexture2D(&desc, pData, &rResource.pTexture);
		assert(hr == S_OK);

		//if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
			viewDesc.Format = getD3DFormat(rTexInfo.format);

			if (rTexInfo.arraySize > 1)
			{
				viewDesc.TextureCubeArray.First2DArrayFace = 0;
				viewDesc.TextureCubeArray.MipLevels = rTexInfo.mipLevels;
				viewDesc.TextureCubeArray.MostDetailedMip = 0;
				viewDesc.TextureCubeArray.NumCubes = rTexInfo.arraySize;
				viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
			}
			else
			{
				viewDesc.TextureCube.MipLevels = rTexInfo.mipLevels;
				viewDesc.TextureCube.MostDetailedMip = 0;
				viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
			}

			hr = pDevice->CreateShaderResourceView(rResource.pTexture, &viewDesc, &rResource.resourceView.pViewD3D11);
			assert(hr == S_OK);
		}

		return true;
	}

	bool CreateTexture2D(ID3D11Device* pDevice, D3D11_SUBRESOURCE_DATA* pData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResource& rResource)
	{
		bool bIsMultisampled = (rTexInfo.sampleCount > 1);
		bool bIsArray = (rTexInfo.arraySize > 1);
		bool bCanBindAccessView = !bIsMultisampled && !resourceFormatIsCompressed(rTexInfo.format) && eUsage != RdrResourceUsage::Immutable;

		D3D11_TEXTURE2D_DESC desc = { 0 };

		desc.Usage = getD3DUsage(eUsage);
		desc.CPUAccessFlags = (desc.Usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE : 0;
		desc.ArraySize = rTexInfo.arraySize;
		desc.MiscFlags = 0;
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

		desc.SampleDesc.Count = rTexInfo.sampleCount;
		desc.SampleDesc.Quality = 0;
		desc.Format = getD3DTypelessFormat(rTexInfo.format);
		desc.Width = rTexInfo.width;
		desc.Height = rTexInfo.height;
		desc.MipLevels = rTexInfo.mipLevels;

		HRESULT hr = pDevice->CreateTexture2D(&desc, pData, &rResource.pTexture);
		assert(hr == S_OK);

		//if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
			viewDesc.Format = getD3DFormat(rTexInfo.format);
			if (bIsArray)
			{
				if (bIsMultisampled)
				{
					viewDesc.Texture2DMSArray.ArraySize = rTexInfo.arraySize;
					viewDesc.Texture2DMSArray.FirstArraySlice = 0;
					viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
				}
				else
				{
					viewDesc.Texture2DArray.ArraySize = rTexInfo.arraySize;
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

			hr = pDevice->CreateShaderResourceView(rResource.pTexture, &viewDesc, &rResource.resourceView.pViewD3D11);
			assert(hr == S_OK);
		}

		if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc;
			viewDesc.Format = getD3DFormat(rTexInfo.format);
			if (bIsArray)
			{
				viewDesc.Texture2DArray.ArraySize = rTexInfo.arraySize;
				viewDesc.Texture2DArray.FirstArraySlice = 0;
				viewDesc.Texture2DArray.MipSlice = 0;
				viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			}
			else
			{
				viewDesc.Texture2D.MipSlice = 0;
				viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			}

			hr = pDevice->CreateUnorderedAccessView(rResource.pTexture, &viewDesc, &rResource.uav.pViewD3D11);
			assert(hr == S_OK);
		}

		return true;
	}
}

bool RdrContextD3D11::CreateTexture(const void* pSrcData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResource& rResource)
{
	uint sliceCount = rTexInfo.bCubemap ? rTexInfo.arraySize * 6 : rTexInfo.arraySize;
	D3D11_SUBRESOURCE_DATA* pData = nullptr;  
	
	if (pSrcData)
	{
		uint resIndex = 0;
		pData = (D3D11_SUBRESOURCE_DATA*)_malloca(sliceCount * rTexInfo.mipLevels * sizeof(D3D11_SUBRESOURCE_DATA));

		char* pPos = (char*)pSrcData;
		assert(rTexInfo.width == rTexInfo.height); // todo: support non-square textures
		for (uint slice = 0; slice < sliceCount; ++slice)
		{
			uint mipWidth = rTexInfo.width;
			uint mipHeight = rTexInfo.height;
			for (uint mip = 0; mip < rTexInfo.mipLevels; ++mip)
			{
				pData[resIndex].pSysMem = pPos;
				pData[resIndex].SysMemPitch = GetTexturePitch(mipWidth, rTexInfo.format);
				pPos += pData[resIndex].SysMemPitch * GetTextureRows(mipHeight, rTexInfo.format);
				mipWidth >>= 1;
				mipHeight >>= 1;
				++resIndex;
			}
		}
	}

	bool res = false;
	if (rTexInfo.bCubemap)
	{
		res = CreateTextureCube(m_pDevice, pData, rTexInfo, eUsage, rResource);
	}
	else
	{
		res = CreateTexture2D(m_pDevice, pData, rTexInfo, eUsage, rResource);
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

void RdrContextD3D11::ResolveSurface(const RdrResource& rSrc, const RdrResource& rDst)
{
	m_pDevContext->ResolveSubresource(rDst.pResource, 0, rSrc.pResource, 0, getD3DFormat(rSrc.texInfo.format));
}

RdrDepthStencilView RdrContextD3D11::CreateDepthStencilView(const RdrResource& rDepthTex)
{
	RdrDepthStencilView view;

	bool bIsMultisampled = (rDepthTex.texInfo.sampleCount > 1);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = getD3DDepthFormat(rDepthTex.texInfo.format);
	if (rDepthTex.texInfo.arraySize > 1)
	{
		if (bIsMultisampled)
		{
			dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
			dsvDesc.Texture2DMSArray.ArraySize = rDepthTex.texInfo.arraySize;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
		}
		else
		{
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.ArraySize = rDepthTex.texInfo.arraySize;
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

	HRESULT hr = m_pDevice->CreateDepthStencilView(rDepthTex.pTexture, &dsvDesc, &view.pView);
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

	HRESULT hr = m_pDevice->CreateDepthStencilView(rDepthTex.pTexture, &dsvDesc, &view.pView);
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

	HRESULT hr = m_pDevice->CreateRenderTargetView(rTexRes.pTexture, &desc, &view.pView);
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

	HRESULT hr = m_pDevice->CreateRenderTargetView(rTexArrayRes.pTexture, &desc, &view.pView);
	assert(hr == S_OK);

	return view;
}

void RdrContextD3D11::ReleaseRenderTargetView(const RdrRenderTargetView& renderTargetView)
{
	renderTargetView.pView->Release();
}

void RdrContextD3D11::SetDepthStencilState(RdrDepthTestMode eDepthTest)
{
	if (!m_pDepthStencilStates[(int)eDepthTest])
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
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

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

void RdrContextD3D11::SetBlendState(const bool bAlphaBlend)
{
	if (!m_pBlendStates[bAlphaBlend])
	{
		D3D11_BLEND_DESC blend_desc = { 0 };
		blend_desc.AlphaToCoverageEnable = false;
		blend_desc.IndependentBlendEnable = false;

		if (bAlphaBlend)
		{
			blend_desc.RenderTarget[0].BlendEnable = true;
			blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
		else
		{
			blend_desc.RenderTarget[0].BlendEnable = false;
			blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}

		ID3D11BlendState* pBlendState = nullptr;
		HRESULT hr = m_pDevice->CreateBlendState(&blend_desc, &pBlendState);
		assert(hr == S_OK);

		m_pBlendStates[bAlphaBlend] = pBlendState;
	}

	m_pDevContext->OMSetBlendState(m_pBlendStates[bAlphaBlend], nullptr, 0xffffffff);
}

void RdrContextD3D11::SetRasterState(const RdrRasterState& rasterState)
{
	uint rasterIndex =
		rasterState.bEnableMSAA * 2 * 2
		+ rasterState.bEnableScissor * 2
		+ rasterState.bWireframe;

	if (!m_pRasterStates[rasterIndex])
	{
		D3D11_RASTERIZER_DESC desc;
		desc.AntialiasedLineEnable = !rasterState.bEnableMSAA;
		desc.MultisampleEnable = rasterState.bEnableMSAA;
		desc.CullMode = D3D11_CULL_BACK;
		desc.DepthBias = 0;
		desc.SlopeScaledDepthBias = 0.f;
		desc.DepthBiasClamp = 0.f;
		desc.DepthClipEnable = true;
		desc.FrontCounterClockwise = false;
		desc.FillMode = rasterState.bWireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
		desc.ScissorEnable = rasterState.bEnableScissor;

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
	HRESULT hr = m_pSwapChain->Present(0, m_presentFlags);
	if (hr == DXGI_STATUS_OCCLUDED)
	{
		// Window is no longer visible, most likely minimized.  
		// Set test flag so that we don't waste time presenting until the window is visible again.
		m_presentFlags |= DXGI_PRESENT_TEST;
	}
	else
	{
		m_presentFlags &= ~DXGI_PRESENT_TEST;
		assert(hr == S_OK);
	}
}

void RdrContextD3D11::Resize(uint width, uint height)
{
	if (m_pPrimaryRenderTarget)
		m_pPrimaryRenderTarget->Release();
	m_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

	ID3D11Texture2D* pBackBuffer;
	m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);

	D3D11_TEXTURE2D_DESC bbDesc;
	pBackBuffer->GetDesc(&bbDesc);

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = bbDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

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

	ID3D11Buffer* pBuffer;
	HRESULT hr = m_pDevice->CreateBuffer(&desc, (pData ? &data : nullptr), &pBuffer);
	assert(hr == S_OK);

	RdrConstantBufferDeviceObj devObj;
	devObj.pBufferD3D11 = pBuffer;
	return devObj;
}

void RdrContextD3D11::UpdateConstantBuffer(RdrConstantBuffer& buffer, const void* pSrcData)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = m_pDevContext->Map(buffer.bufferObj.pBufferD3D11, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	memcpy(mapped.pData, pSrcData, buffer.size);

	m_pDevContext->Unmap(buffer.bufferObj.pBufferD3D11, 0);
}

void RdrContextD3D11::ReleaseConstantBuffer(const RdrConstantBufferDeviceObj& buffer)
{
	buffer.pBufferD3D11->Release();
}

void RdrContextD3D11::Draw(const RdrDrawState& rDrawState)
{
	static_assert(sizeof(RdrConstantBufferDeviceObj) == sizeof(ID3D11Buffer*), "RdrConstantBufferDeviceObj must only contain the device obj pointer.");

	m_pDevContext->VSSetShader(rDrawState.pVertexShader->pVertex, nullptr, 0);
	m_pDevContext->VSSetConstantBuffers(0, rDrawState.vsConstantBufferCount, (ID3D11Buffer**)rDrawState.vsConstantBuffers);

	if (rDrawState.pGeometryShader)
	{
		m_pDevContext->GSSetShader(rDrawState.pGeometryShader->pGeometry, nullptr, 0);
		m_pDevContext->GSSetConstantBuffers(0, rDrawState.gsConstantBufferCount, (ID3D11Buffer**)rDrawState.gsConstantBuffers);
	}
	else
	{
		m_pDevContext->GSSetShader(nullptr, nullptr, 0);
	}

	if (rDrawState.pPixelShader)
	{
		m_pDevContext->PSSetShader(rDrawState.pPixelShader->pPixel, nullptr, 0);
		m_pDevContext->PSSetConstantBuffers(0, rDrawState.psConstantBufferCount, (ID3D11Buffer**)rDrawState.psConstantBuffers);

		uint numResources = ARRAYSIZE(rDrawState.psResources);
		for (uint i = 0; i < numResources; ++i)
		{
			ID3D11ShaderResourceView* pResource = rDrawState.psResources[i].pViewD3D11;
			m_pDevContext->PSSetShaderResources(i, 1, &pResource);
		}

		uint numSamplers = ARRAYSIZE(rDrawState.psSamplers);
		for (uint i = 0; i < numSamplers; ++i)
		{
			ID3D11SamplerState* pSampler = GetSampler(rDrawState.psSamplers[i]);
			m_pDevContext->PSSetSamplers(i, 1, &pSampler);
		}
	}
	else
	{
		m_pDevContext->PSSetShader(nullptr, nullptr, 0);
	}

	m_pDevContext->IASetInputLayout(rDrawState.inputLayout.pInputLayout);
	m_pDevContext->IASetPrimitiveTopology(getD3DTopology(rDrawState.eTopology));

	m_pDevContext->IASetVertexBuffers(0, 1, &rDrawState.vertexBuffers[0].pBuffer, &rDrawState.vertexStride, &rDrawState.vertexOffset);

	if (rDrawState.indexBuffer.pBuffer)
	{
		m_pDevContext->IASetIndexBuffer(rDrawState.indexBuffer.pBuffer, DXGI_FORMAT_R16_UINT, 0);
		m_pDevContext->DrawIndexed(rDrawState.indexCount, 0, 0);
	}
	else
	{
		m_pDevContext->Draw(rDrawState.vertexCount, 0);
	}
}

void RdrContextD3D11::DispatchCompute(const RdrDrawState& rDrawState, uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ)
{
	m_pDevContext->CSSetShader(rDrawState.pComputeShader->pCompute, nullptr, 0);

	m_pDevContext->CSSetConstantBuffers(0, rDrawState.csConstantBufferCount, (ID3D11Buffer**)rDrawState.csConstantBuffers);

	uint numResources = ARRAYSIZE(rDrawState.csResources);
	for (uint i = 0; i < numResources; ++i)
	{
		ID3D11ShaderResourceView* pResource = rDrawState.csResources[i].pViewD3D11;
		m_pDevContext->CSSetShaderResources(i, 1, &pResource);
	}

	uint numUavs = ARRAYSIZE(rDrawState.csUavs);
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
}

void RdrContextD3D11::PSClearResources()
{
	ID3D11ShaderResourceView* resourceViews[16] = { 0 };
	m_pDevContext->PSSetShaderResources(0, 16, resourceViews);
}
