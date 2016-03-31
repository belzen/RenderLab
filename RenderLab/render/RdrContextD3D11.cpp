#include "Precompiled.h"
#include "RdrContextD3D11.h"
#include "RdrDrawOp.h"
#include "Light.h"

#include <d3d11.h>
#include <d3d11_1.h>

#include <DirectXTex/include/DirectXTex.h>
#include <DirectXTex/include/Dds.h>

#pragma comment (lib, "DirectXTex.lib")
#pragma comment (lib, "d3d11.lib")

namespace
{
	static bool s_useDebugDevice = 1;

	enum RdrReservedPsResourceSlots
	{
		kPsResource_ShadowMaps = 14,
		kPsResource_ShadowCubeMaps = 15,
		kPsResource_LightList = 16,
		kPsResource_TileLightIds = 17,
		kPsResource_ShadowMapData = 18,
	};

	enum RdrReservedPsSamplerSlots
	{
		kPsSampler_ShadowMap = 15,
	};

	bool resourceFormatIsDepth(RdrResourceFormat format)
	{
		return format == kResourceFormat_D16 || format == kResourceFormat_D24_UNORM_S8_UINT;
	}

	DXGI_FORMAT getD3DFormat(RdrResourceFormat format)
	{
		static const DXGI_FORMAT s_d3dFormats[] = {
			DXGI_FORMAT_R16_UNORM,				// kResourceFormat_D16
			DXGI_FORMAT_R24_UNORM_X8_TYPELESS,	// kResourceFormat_D24_UNORM_S8_UINT
			DXGI_FORMAT_R16_UNORM,				// kResourceFormat_R16_UNORM
			DXGI_FORMAT_R16G16_FLOAT,			// kResourceFormat_RG_F16
		};
		static_assert(ARRAYSIZE(s_d3dFormats) == kResourceFormat_Count, "Missing D3D formats!");
		return s_d3dFormats[format];
	}

	DXGI_FORMAT getD3DDepthFormat(RdrResourceFormat format)
	{
		static const DXGI_FORMAT s_d3dDepthFormats[] = {
			DXGI_FORMAT_D16_UNORM,			// kResourceFormat_D16
			DXGI_FORMAT_D24_UNORM_S8_UINT,	// kResourceFormat_D24_UNORM_S8_UINT
			DXGI_FORMAT_UNKNOWN,			// kResourceFormat_R16_UNORM
			DXGI_FORMAT_UNKNOWN,			// kResourceFormat_RG_F16
		};
		static_assert(ARRAYSIZE(s_d3dDepthFormats) == kResourceFormat_Count, "Missing D3D depth formats!");
		assert(s_d3dDepthFormats[format] != DXGI_FORMAT_UNKNOWN);
		return s_d3dDepthFormats[format];
	}

	DXGI_FORMAT getD3DTypelessFormat(RdrResourceFormat format)
	{
		static const DXGI_FORMAT s_d3dTypelessFormats[] = {
			DXGI_FORMAT_R16_TYPELESS,		// kResourceFormat_D16
			DXGI_FORMAT_R24G8_TYPELESS,		// kResourceFormat_D24_UNORM_S8_UINT
			DXGI_FORMAT_R16_TYPELESS,		// kResourceFormat_R16_UNORM
			DXGI_FORMAT_R16G16_TYPELESS,	// kResourceFormat_RG_F16
		};
		static_assert(ARRAYSIZE(s_d3dTypelessFormats) == kResourceFormat_Count, "Missing typeless formats!");
		return s_d3dTypelessFormats[format];
	}

	uint getD3DCpuAccessFlags(RdrCpuAccessFlags cpuAccessFlags)
	{
		uint d3dFlags = 0;
		if (cpuAccessFlags & kRdrCpuAccessFlag_Read)
			d3dFlags |= D3D11_CPU_ACCESS_READ;
		if (cpuAccessFlags & kRdrCpuAccessFlag_Write)
			d3dFlags |= D3D11_CPU_ACCESS_WRITE;
		return d3dFlags;
	}

	D3D11_USAGE getD3DUsage(RdrResourceUsage eUsage)
	{
		static const D3D11_USAGE s_d3dUsage[] = {
			D3D11_USAGE_DEFAULT,	// kRdrResourceUsage_Default
			D3D11_USAGE_IMMUTABLE,	// kRdrResourceUsage_Immutable
			D3D11_USAGE_DYNAMIC,	// kRdrResourceUsage_Dynamic
			D3D11_USAGE_STAGING,	// kRdrResourceUsage_Staging
		};
		static_assert(ARRAYSIZE(s_d3dUsage) == kRdrResourceUsage_Count, "Missing D3D11 resource usage!");
		return s_d3dUsage[eUsage];
	}

	static ID3D11Buffer* createBuffer(ID3D11Device* pDevice, const void* pSrcData, int size, uint bindFlags)
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

RdrResourceHandle RdrContextD3D11::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize)
{
	D3D11_BUFFER_DESC desc = { 0 };
	D3D11_SUBRESOURCE_DATA data = { 0 };

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = numElements * elementSize;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.StructureByteStride = elementSize;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	data.pSysMem = pSrcData;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	RdrResource* pRes = m_resources.alloc();

	HRESULT hr = m_pDevice->CreateBuffer(&desc, (pSrcData ? &data : nullptr), (ID3D11Buffer**)&pRes->pResource);
	assert(hr == S_OK);
	hr = m_pDevice->CreateUnorderedAccessView(pRes->pResource, nullptr, &pRes->pUnorderedAccessView);
	assert(hr == S_OK);
	hr = m_pDevice->CreateShaderResourceView(pRes->pResource, nullptr, &pRes->pResourceView);
	assert(hr == S_OK);

	return m_resources.getId(pRes);
}

RdrResourceHandle RdrContextD3D11::CreateVertexBuffer(const void* vertices, int size)
{
	RdrResource* pVertexBuffer = m_resources.alloc();
	pVertexBuffer->pResource = createBuffer(m_pDevice, vertices, size, D3D11_BIND_VERTEX_BUFFER);
	return m_resources.getId(pVertexBuffer);
}

RdrResourceHandle RdrContextD3D11::CreateIndexBuffer(const void* indices, int size)
{
	RdrResource* pIndexBuffer = m_resources.alloc();
	pIndexBuffer->pResource = createBuffer(m_pDevice, indices, size, D3D11_BIND_INDEX_BUFFER);
	return m_resources.getId(pIndexBuffer);
}

static int GetTexturePitch(int width, DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8_UNORM:
		return width * 1;
	case DXGI_FORMAT_BC1_UNORM:
		return ((width + 3) & ~3) * 2;
	case DXGI_FORMAT_BC3_UNORM:
		return ((width + 3) & ~3) * 4;
	default:
		assert(false);
		return 0;
	}
}

static int GetTextureRows(int height, DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8_UNORM:
		return height;
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC3_UNORM:
		return ((height + 3) & ~3) / 4;
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
	static_assert(ARRAYSIZE(cmpFuncD3d) == kComparisonFunc_Count, "Missing comparison func");
	return cmpFuncD3d[cmpFunc];
}

static D3D11_TEXTURE_ADDRESS_MODE getTexCoordModeD3d(const RdrTexCoordMode uvMode)
{
	D3D11_TEXTURE_ADDRESS_MODE uvModeD3d[] = {
		D3D11_TEXTURE_ADDRESS_WRAP,
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_TEXTURE_ADDRESS_MIRROR
	};
	static_assert(ARRAYSIZE(uvModeD3d) == kRdrTexCoordMode_Count, "Missing tex coord mode");
	return uvModeD3d[uvMode];
}

static D3D11_FILTER getFilterD3d(const RdrComparisonFunc cmpFunc, const bool bPointSample)
{
	if (cmpFunc != kComparisonFunc_Never)
	{
		return bPointSample ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	}
	else
	{
		return bPointSample ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	}
}

bool RdrContextD3D11::Init(HWND hWnd, uint width, uint height, uint msaaLevel)
{
	HRESULT hr;

	m_msaaLevel = msaaLevel;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SampleDesc.Count = msaaLevel;
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

ID3D11SamplerState* RdrContextD3D11::GetSampler(const RdrSamplerState& state)
{
	uint samplerIndex = 
		state.cmpFunc * (kRdrTexCoordMode_Count * 2)
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

RdrGeoHandle RdrContextD3D11::CreateGeometry(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size)
{
	RdrGeometry* pGeo = m_geo.alloc();

	pGeo->numVerts = numVerts;
	pGeo->numIndices = numIndices;
	pGeo->hVertexBuffer = CreateVertexBuffer(pVertData, vertStride * pGeo->numVerts);
	pGeo->hIndexBuffer = CreateIndexBuffer(pIndexData, sizeof(uint16) * pGeo->numIndices);
	pGeo->vertStride = vertStride;
	pGeo->size = size;
	pGeo->radius = Vec3Length(size);

	return m_geo.getId(pGeo);
}

void RdrContextD3D11::ReleaseGeometry(RdrGeoHandle hGeo)
{
	RdrGeometry* pGeo = m_geo.get(hGeo);
	ReleaseResource(pGeo->hIndexBuffer);
	ReleaseResource(pGeo->hVertexBuffer);
	m_geo.releaseId(hGeo);
}

RdrResourceHandle RdrContextD3D11::LoadTexture(const char* filename)
{
	RdrTextureMap::iterator iter = m_textureCache.find(filename);
	if (iter != m_textureCache.end())
		return iter->second;

	D3D11_TEXTURE2D_DESC desc = { 0 };
	D3D11_SUBRESOURCE_DATA data[16] = { 0 };

	desc.ArraySize = 1;
	desc.CPUAccessFlags = 0;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.SampleDesc.Count = 1;

	char fullFilename[MAX_PATH];
	sprintf_s(fullFilename, "data/textures/%s", filename);

	FILE* file;
	fopen_s(&file, fullFilename, "rb");

	fseek(file, 0, SEEK_END);
	int fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* pTexData = new char[fileSize];
	char* pPos = pTexData;
	fread(pTexData, sizeof(char), fileSize, file);
	{
		DirectX::TexMetadata metadata;
		DirectX::GetMetadataFromDDSMemory(pTexData, fileSize, 0, metadata);

		desc.Format = metadata.format;
		desc.Width = metadata.height;
		desc.Height = metadata.width;
		desc.MipLevels = metadata.mipLevels;

		pPos += sizeof(DWORD); // magic?
		pPos += sizeof(DirectX::DDS_HEADER);

		int width = desc.Height;
		int height = desc.Width;
		assert(desc.Width == desc.Height);
		for (uint i = 0; i < desc.MipLevels; ++i)
		{
			data[i].pSysMem = pPos;
			data[i].SysMemPitch = GetTexturePitch(width, desc.Format);
			pPos += data[i].SysMemPitch * GetTextureRows(height, desc.Format);
			width >>= 1;
			height >>= 1;
		}
	}
	fclose(file);

	ID3D11Texture2D* pTexture;
	HRESULT hr = m_pDevice->CreateTexture2D(&desc, data, &pTexture);
	assert(hr == S_OK);

	delete pTexData;

	RdrResource* pTex = m_resources.alloc();

	pTex->pTexture = pTexture;
	pTex->width = desc.Width;
	pTex->height = desc.Height;

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = desc.Format;
		viewDesc.Texture2D.MipLevels = desc.MipLevels;
		viewDesc.Texture2D.MostDetailedMip = 0;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		hr = m_pDevice->CreateShaderResourceView(pTexture, &viewDesc, &pTex->pResourceView);
		assert(hr == S_OK);
	}

	RdrResourceHandle hTex = m_resources.getId(pTex);
	m_textureCache.insert(std::make_pair(filename, hTex));
	return hTex;
}

void RdrContextD3D11::ReleaseResource(RdrResourceHandle hRes)
{
	RdrResource* pRes = m_resources.get(hRes);
	if (pRes->pResource)
		pRes->pResource->Release();
	if (pRes->pResourceView)
		pRes->pResourceView->Release();
	if (pRes->pUnorderedAccessView)
		pRes->pUnorderedAccessView->Release();

	m_resources.releaseId(hRes);
}

RdrResourceHandle RdrContextD3D11::CreateTexture2D(uint width, uint height, RdrResourceFormat format)
{
	return CreateTexture2DMS(width, height, format, 1);
}

RdrResourceHandle RdrContextD3D11::CreateTexture2DMS(uint width, uint height, RdrResourceFormat format, uint sampleCount)
{
	bool bIsMultisampled = (sampleCount > 1);
	D3D11_TEXTURE2D_DESC desc = { 0 };

	desc.ArraySize = 1;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (resourceFormatIsDepth(format))
		desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	else if (!bIsMultisampled)
		desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.SampleDesc.Count = sampleCount;
	desc.SampleDesc.Quality = 0;
	desc.Format = getD3DTypelessFormat(format);
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;

	ID3D11Texture2D* pTexture;
	HRESULT hr = m_pDevice->CreateTexture2D(&desc, nullptr, &pTexture);
	assert(hr == S_OK);

	RdrResource* pTex = m_resources.alloc();
	pTex->pTexture = pTexture;
	pTex->width = width;
	pTex->height = height;

	//if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(format);
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Texture2D.MostDetailedMip = 0;
		viewDesc.ViewDimension = bIsMultisampled ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;

		hr = m_pDevice->CreateShaderResourceView(pTexture, &viewDesc, &pTex->pResourceView);
		assert(hr == S_OK);
	}

	if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(format);
		viewDesc.Texture2D.MipSlice = 0;
		viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

		hr = m_pDevice->CreateUnorderedAccessView(pTexture, &viewDesc, &pTex->pUnorderedAccessView);
		assert(hr == S_OK);
	}

	return m_resources.getId(pTex);
}

RdrResourceHandle RdrContextD3D11::CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat format)
{
	D3D11_TEXTURE2D_DESC desc = { 0 };

	desc.ArraySize = arraySize;
	desc.CPUAccessFlags = 0;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (resourceFormatIsDepth(format))
		desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.SampleDesc.Count = 1;
	desc.Format = getD3DTypelessFormat(format);
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;

	ID3D11Texture2D* pTexture;
	HRESULT hr = m_pDevice->CreateTexture2D(&desc, nullptr, &pTexture);
	assert(hr == S_OK);

	RdrResource* pTex = m_resources.alloc();
	pTex->pTexture = pTexture;

	//if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(format);
		viewDesc.Texture2DArray.ArraySize = arraySize;
		viewDesc.Texture2DArray.FirstArraySlice = 0;
		viewDesc.Texture2DArray.MipLevels = 1;
		viewDesc.Texture2DArray.MostDetailedMip = 0;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;

		hr = m_pDevice->CreateShaderResourceView(pTexture, &viewDesc, &pTex->pResourceView);
		assert(hr == S_OK);
	}

	return m_resources.getId(pTex);
}

RdrResourceHandle RdrContextD3D11::CreateTextureCube(uint width, uint height, RdrResourceFormat format)
{
	static const uint kCubemapArraySize = 6;
	D3D11_TEXTURE2D_DESC desc = { 0 };

	desc.ArraySize = kCubemapArraySize;
	desc.CPUAccessFlags = 0;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (resourceFormatIsDepth(format))
		desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.SampleDesc.Count = 1;
	desc.Format = getD3DTypelessFormat(format);
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	ID3D11Texture2D* pTexture;
	HRESULT hr = m_pDevice->CreateTexture2D(&desc, nullptr, &pTexture);
	assert(hr == S_OK);

	RdrResource* pTex = m_resources.alloc();
	pTex->pTexture = pTexture;

	//if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(format);
		viewDesc.Texture2DArray.ArraySize = kCubemapArraySize;
		viewDesc.Texture2DArray.FirstArraySlice = 0;
		viewDesc.Texture2DArray.MipLevels = 1;
		viewDesc.Texture2DArray.MostDetailedMip = 0;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;

		hr = m_pDevice->CreateShaderResourceView(pTexture, &viewDesc, &pTex->pResourceView);
		assert(hr == S_OK);
	}

	return m_resources.getId(pTex);
}

RdrResourceHandle RdrContextD3D11::CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat format)
{
	const uint cubemapArraySize = arraySize * 6;
	D3D11_TEXTURE2D_DESC desc = { 0 };

	desc.ArraySize = cubemapArraySize;
	desc.CPUAccessFlags = 0;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (resourceFormatIsDepth(format))
		desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	else
		desc.BindFlags |= D3D11_BIND_RENDER_TARGET;

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.SampleDesc.Count = 1;
	desc.Format = getD3DTypelessFormat(format);
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	ID3D11Texture2D* pTexture;
	HRESULT hr = m_pDevice->CreateTexture2D(&desc, nullptr, &pTexture);
	assert(hr == S_OK);

	RdrResource* pTex = m_resources.alloc();
	pTex->pTexture = pTexture;

	//if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(format);
		viewDesc.TextureCubeArray.First2DArrayFace = 0;
		viewDesc.TextureCubeArray.MipLevels = 1;
		viewDesc.TextureCubeArray.MostDetailedMip = 0;
		viewDesc.TextureCubeArray.NumCubes = arraySize;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;

		hr = m_pDevice->CreateShaderResourceView(pTexture, &viewDesc, &pTex->pResourceView);
		assert(hr == S_OK);
	}

	return m_resources.getId(pTex);
}

RdrDepthStencilView RdrContextD3D11::CreateDepthStencilView(RdrResourceHandle hDepthTex, RdrResourceFormat format, bool bMultisampled)
{
	RdrResource* pDepthTex = m_resources.get(hDepthTex);

	RdrDepthStencilView view;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = getD3DDepthFormat(format);
	dsvDesc.ViewDimension = bMultisampled ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	HRESULT hr = m_pDevice->CreateDepthStencilView(pDepthTex->pTexture, &dsvDesc, &view.pView);
	assert(hr == S_OK);

	return view;
}

RdrDepthStencilView RdrContextD3D11::CreateDepthStencilView(RdrResourceHandle hDepthTexArray, int arrayIndex, RdrResourceFormat format)
{
	RdrDepthStencilView view;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = DXGI_FORMAT_D16_UNORM;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	dsvDesc.Texture2DArray.MipSlice = 0;
	dsvDesc.Texture2DArray.FirstArraySlice = arrayIndex;
	dsvDesc.Texture2DArray.ArraySize = 1;

	RdrResource* pShadowTex = m_resources.get(hDepthTexArray);
	HRESULT hr = m_pDevice->CreateDepthStencilView(pShadowTex->pTexture, &dsvDesc, &view.pView);
	assert(hr == S_OK);

	return view;
}

void RdrContextD3D11::ReleaseDepthStencilView(RdrDepthStencilView depthStencilView)
{
	depthStencilView.pView->Release();
}

RdrRenderTargetView RdrContextD3D11::CreateRenderTargetView(RdrResourceHandle hTexArrayRes, int arrayIndex, RdrResourceFormat format)
{
	RdrRenderTargetView view;

	D3D11_RENDER_TARGET_VIEW_DESC desc;
	desc.Format = getD3DFormat(format);
	desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	desc.Texture2DArray.MipSlice = 0;
	desc.Texture2DArray.FirstArraySlice = arrayIndex;
	desc.Texture2DArray.ArraySize = 1;

	RdrResource* pTex = m_resources.get(hTexArrayRes);
	HRESULT hr = m_pDevice->CreateRenderTargetView(pTex->pTexture, &desc, &view.pView);
	assert(hr == S_OK);

	return view;
}

void RdrContextD3D11::DrawGeo(RdrDrawOp* pDrawOp, RdrShaderMode eShaderMode, const LightList* pLightList, RdrResourceHandle hTileLightIndices)
{
	bool bDepthOnly = (eShaderMode == kRdrShaderMode_DepthOnly || eShaderMode == kRdrShaderMode_CubeMapDepthOnly);
	RdrGeometry* pGeo = m_geo.get(pDrawOp->hGeo);
	UINT stride = pGeo->vertStride;
	UINT offset = 0;
	ID3D11Buffer* pConstantsBuffer = nullptr;
	HRESULT hr;

	if (pDrawOp->numConstants)
	{
		D3D11_BUFFER_DESC desc = { 0 };
		D3D11_SUBRESOURCE_DATA data = { 0 };

		desc.ByteWidth = pDrawOp->numConstants * sizeof(Vec4);
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.Usage = D3D11_USAGE_IMMUTABLE;

		data.pSysMem = pDrawOp->constants;

		hr = m_pDevice->CreateBuffer(&desc, &data, &pConstantsBuffer);
		assert(hr == S_OK);

		m_pDevContext->VSSetConstantBuffers(1, 1, &pConstantsBuffer);
	}

	RdrVertexShader* pVertexShader = m_vertexShaders.get(pDrawOp->hVertexShaders[eShaderMode]);
	m_pDevContext->VSSetShader(pVertexShader->pShader, nullptr, 0);

	if (pDrawOp->hPixelShaders[eShaderMode])
	{
		RdrPixelShader* pPixelShader = m_pixelShaders.get(pDrawOp->hPixelShaders[eShaderMode]);
		m_pDevContext->PSSetShader(pPixelShader->pShader, nullptr, 0);
	}
	else
	{
		m_pDevContext->PSSetShader(nullptr, nullptr, 0);
	}

	if (pDrawOp->hGeometryShaders[eShaderMode])
	{
		RdrGeometryShader* pGeometryShader = m_geometryShaders.get(pDrawOp->hGeometryShaders[eShaderMode]);
		m_pDevContext->GSSetShader(pGeometryShader->pShader, nullptr, 0);
	}
	else
	{
		m_pDevContext->GSSetShader(nullptr, nullptr, 0);
	}

	for (uint i = 0; i < pDrawOp->texCount; ++i)
	{
		RdrResource* pTex = m_resources.get(pDrawOp->hTextures[i]);
		ID3D11SamplerState* pSampler = GetSampler(pDrawOp->samplers[i]);
		m_pDevContext->PSSetShaderResources(i, 1, &pTex->pResourceView);
		m_pDevContext->PSSetSamplers(i, 1, &pSampler);
	}

	if (pDrawOp->needsLighting && !bDepthOnly)
	{
		RdrResource* pTex = m_resources.get(pLightList->GetLightListRes());
		m_pDevContext->PSSetShaderResources(kPsResource_LightList, 1, &pTex->pResourceView);

		pTex = m_resources.get(hTileLightIndices);
		m_pDevContext->PSSetShaderResources(kPsResource_TileLightIds, 1, &pTex->pResourceView);

		pTex = m_resources.get(pLightList->GetShadowMapTexArray());
		m_pDevContext->PSSetShaderResources(kPsResource_ShadowMaps, 1, &pTex->pResourceView);

		pTex = m_resources.get(pLightList->GetShadowCubeMapTexArray());
		m_pDevContext->PSSetShaderResources(kPsResource_ShadowCubeMaps, 1, &pTex->pResourceView);

		RdrSamplerState shadowSamplerState(kComparisonFunc_LessEqual, kRdrTexCoordMode_Clamp, false);
		ID3D11SamplerState* pSampler = GetSampler(shadowSamplerState);
		m_pDevContext->PSSetSamplers(kPsSampler_ShadowMap, 1, &pSampler);

		pTex = m_resources.get(pLightList->GetShadowMapDataRes());
		m_pDevContext->PSSetShaderResources(kPsResource_ShadowMapData, 1, &pTex->pResourceView);
	}

	m_pDevContext->IASetInputLayout(pVertexShader->pInputLayout);
	m_pDevContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	RdrResource* pVertexBuffer = m_resources.get(pGeo->hVertexBuffer);
	m_pDevContext->IASetVertexBuffers(0, 1, &pVertexBuffer->pBuffer, &stride, &offset);

	if (pGeo->hIndexBuffer)
	{
		RdrResource* pIndexBuffer = m_resources.get(pGeo->hIndexBuffer);
		m_pDevContext->IASetIndexBuffer(pIndexBuffer->pBuffer, DXGI_FORMAT_R16_UINT, 0);
		m_pDevContext->DrawIndexed(pGeo->numIndices, 0, 0);
	}
	else
	{
		m_pDevContext->Draw(pGeo->numVerts, 0);
	}

	if (pConstantsBuffer)
		pConstantsBuffer->Release();


	ID3D11ShaderResourceView* pNullResourceView = nullptr;
	m_pDevContext->PSSetShaderResources(kPsResource_LightList, 1, &pNullResourceView);
	m_pDevContext->PSSetShaderResources(kPsResource_TileLightIds, 1, &pNullResourceView);
}

void RdrContextD3D11::DispatchCompute(RdrDrawOp* pDrawOp)
{
	RdrComputeShader* pComputeShader = m_computeShaders.get(pDrawOp->hComputeShader);
	ID3D11Buffer* pConstantsBuffer = nullptr;
	HRESULT hr;

	m_pDevContext->CSSetShader(pComputeShader->pShader, nullptr, 0);

	if (pDrawOp->numConstants)
	{
		D3D11_BUFFER_DESC desc = { 0 };
		D3D11_SUBRESOURCE_DATA data = { 0 };

		desc.ByteWidth = pDrawOp->numConstants * sizeof(Vec4);
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.Usage = D3D11_USAGE_IMMUTABLE;

		data.pSysMem = pDrawOp->constants;

		hr = m_pDevice->CreateBuffer(&desc, &data, &pConstantsBuffer);
		assert(hr == S_OK);

		m_pDevContext->CSSetConstantBuffers(0, 1, &pConstantsBuffer);
	}

	for (uint i = 0; i < pDrawOp->texCount; ++i)
	{
		RdrResource* pTex = m_resources.get(pDrawOp->hTextures[i]);
		m_pDevContext->CSSetShaderResources(i, 1, &pTex->pResourceView);
	}

	for (uint i = 0; i < pDrawOp->viewCount; ++i)
	{
		RdrResource* pTex = m_resources.get(pDrawOp->hViews[i]);
		m_pDevContext->CSSetUnorderedAccessViews(i, 1, &pTex->pUnorderedAccessView, nullptr);
	}

	m_pDevContext->Dispatch(pDrawOp->computeThreads[0], pDrawOp->computeThreads[1], pDrawOp->computeThreads[2]);

	// Clear resources to avoid binding errors (input bound as output).  todo: don't do this
	for (uint i = 0; i < pDrawOp->texCount; ++i)
	{
		ID3D11ShaderResourceView* pResourceView = nullptr;
		m_pDevContext->CSSetShaderResources(i, 1, &pResourceView);
	}

	for (uint i = 0; i < pDrawOp->viewCount; ++i)
	{
		ID3D11UnorderedAccessView* pUnorderedAccessView = nullptr;
		m_pDevContext->CSSetUnorderedAccessViews(i, 1, &pUnorderedAccessView, nullptr);
	}

	if (pConstantsBuffer)
		pConstantsBuffer->Release();
}

void RdrContextD3D11::SetDepthStencilState(RdrDepthTestMode eDepthTest)
{
	if (!m_pDepthStencilStates[eDepthTest])
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc;

		dsDesc.DepthEnable = (eDepthTest != kRdrDepthTestMode_None);
		switch (eDepthTest)
		{
		case kRdrDepthTestMode_Less:
			dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
			break;
		case kRdrDepthTestMode_Equal:
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

		m_pDepthStencilStates[eDepthTest] = pState;
	}

	m_pDevContext->OMSetDepthStencilState(m_pDepthStencilStates[eDepthTest], 1);
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

void RdrContextD3D11::SetRenderTargets(uint numTargets, RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget)
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
	m_pSwapChain->Present(0, 0);
}

void RdrContextD3D11::Resize(uint width, uint height)
{
	if (m_pPrimaryRenderTarget)
		m_pPrimaryRenderTarget->Release();
	m_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

	// Release existing resources
	if (m_primaryDepthStencilView.pView)
		ReleaseDepthStencilView(m_primaryDepthStencilView);
	if (m_hPrimaryDepthBuffer)
		ReleaseResource(m_hPrimaryDepthBuffer);

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

	// Depth Buffer
	m_hPrimaryDepthBuffer = CreateTexture2DMS(width, height, kResourceFormat_D24_UNORM_S8_UINT, m_msaaLevel);
	m_primaryDepthStencilView = CreateDepthStencilView(m_hPrimaryDepthBuffer, kResourceFormat_D24_UNORM_S8_UINT, (m_msaaLevel > 1));
	assert(hr == S_OK);
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

RdrDepthStencilView RdrContextD3D11::GetPrimaryDepthStencilTarget()
{
	return m_primaryDepthStencilView;
}

RdrResourceHandle RdrContextD3D11::GetPrimaryDepthTexture()
{
	return m_hPrimaryDepthBuffer;
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

void* RdrContextD3D11::MapResource(RdrResourceHandle hResource, RdrResourceMapMode mapMode)
{
	static const D3D11_MAP s_d3dMapModes[] = {
		D3D11_MAP_READ,					// kRdrResourceMap_Read
		D3D11_MAP_WRITE,				// kRdrResourceMap_Write
		D3D11_MAP_READ_WRITE,			// kRdrResourceMap_ReadWrite
		D3D11_MAP_WRITE_DISCARD,		// kRdrResourceMap_WriteDiscard
		D3D11_MAP_WRITE_NO_OVERWRITE	// kRdrResourceMap_WriteNoOverwrite
	};
	static_assert(ARRAYSIZE(s_d3dMapModes) == kRdrResourceMap_Count, "Missing D3D map modes!");

	RdrResource* pResource = m_resources.get(hResource);

	D3D11_MAPPED_SUBRESOURCE mapped = { 0 };
	HRESULT hr = m_pDevContext->Map(pResource->pResource, 0, s_d3dMapModes[mapMode], 0, &mapped);
	assert(hr == S_OK);

	return mapped.pData;
}

void RdrContextD3D11::UnmapResource(RdrResourceHandle hResource)
{
	RdrResource* pResource = m_resources.get(hResource);
	m_pDevContext->Unmap(pResource->pResource, 0);
}

RdrResourceHandle RdrContextD3D11::CreateConstantBuffer(uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage)
{
	D3D11_BUFFER_DESC desc = { 0 };
	desc.ByteWidth = size;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = getD3DCpuAccessFlags(cpuAccessFlags);
	desc.Usage = getD3DUsage(eUsage);

	RdrResource* pConstantRes = m_resources.alloc();
	HRESULT hr = m_pDevice->CreateBuffer(&desc, nullptr, &pConstantRes->pBuffer);
	assert(hr == S_OK);

	return m_resources.getId(pConstantRes);
}

void RdrContextD3D11::VSSetConstantBuffers(uint startSlot, uint numBuffers, RdrResourceHandle* aConstantBuffers)
{
	ID3D11Buffer* apBuffers[12];
	assert(numBuffers < 12);
	for (uint i = 0; i < numBuffers; ++i)
	{
		RdrResource* pResource = m_resources.get(aConstantBuffers[i]);
		apBuffers[i] = pResource->pBuffer;
	}
	m_pDevContext->VSSetConstantBuffers(startSlot, numBuffers, apBuffers);
}

void RdrContextD3D11::PSSetConstantBuffers(uint startSlot, uint numBuffers, RdrResourceHandle* aConstantBuffers)
{
	ID3D11Buffer* apBuffers[12];
	assert(numBuffers < 12);
	for (uint i = 0; i < numBuffers; ++i)
	{
		RdrResource* pResource = m_resources.get(aConstantBuffers[i]);
		apBuffers[i] = pResource->pBuffer;
	}
	m_pDevContext->PSSetConstantBuffers(startSlot, numBuffers, apBuffers);
}

void RdrContextD3D11::PSClearResources()
{
	ID3D11ShaderResourceView* resourceViews[16] = { 0 };
	m_pDevContext->PSSetShaderResources(0, 16, resourceViews);
}

void RdrContextD3D11::GSSetConstantBuffers(uint startSlot, uint numBuffers, RdrResourceHandle* aConstantBuffers)
{
	ID3D11Buffer* apBuffers[12];
	assert(numBuffers < 12);
	for (uint i = 0; i < numBuffers; ++i)
	{
		RdrResource* pResource = m_resources.get(aConstantBuffers[i]);
		apBuffers[i] = pResource->pBuffer;
	}
	m_pDevContext->GSSetConstantBuffers(startSlot, numBuffers, apBuffers);
}
