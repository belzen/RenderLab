#include "Precompiled.h"
#include "RdrContext.h"
#include <d3d11.h>
#include <assert.h>
#include <stdio.h>
#include <DirectXTex/include/DirectXTex.h>
#include <DirectXTex/include/Dds.h>
#include <list>
#include <fstream>
#include <vector>

#pragma comment (lib, "DirectXTex.lib")

static bool resourceFormatIsDepth(RdrResourceFormat format)
{
	return format == kResourceFormat_D16;
}

static DXGI_FORMAT getD3DFormat(RdrResourceFormat format)
{
	static const DXGI_FORMAT s_d3dFormats[] = {
		DXGI_FORMAT_R16_UNORM,		// kResourceFormat_D16
		DXGI_FORMAT_R16G16_FLOAT,	// kResourceFormat_RG_F16
	};
	static_assert(ARRAYSIZE(s_d3dFormats) == kResourceFormat_Count, "Missing typeless formats!");
	return s_d3dFormats[format];
}

static DXGI_FORMAT getD3DTypelessFormat(RdrResourceFormat format)
{
	static const DXGI_FORMAT s_d3dTypelessFormats[] = {
		DXGI_FORMAT_R16_TYPELESS,		// kResourceFormat_D16
		DXGI_FORMAT_R16G16_TYPELESS,	// kResourceFormat_RG_F16
	};
	static_assert(ARRAYSIZE(s_d3dTypelessFormats) == kResourceFormat_Count, "Missing typeless formats!");
	return s_d3dTypelessFormats[format];
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

RdrTextureHandle RdrContext::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize)
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

	RdrTexture* pTex = m_textures.alloc();

	HRESULT hr = m_pDevice->CreateBuffer(&desc, (pSrcData ? &data : nullptr), (ID3D11Buffer**)&pTex->pResource);
	assert(hr == S_OK);
	hr = m_pDevice->CreateUnorderedAccessView(pTex->pResource, nullptr, &pTex->pUnorderedAccessView);
	assert(hr == S_OK);
	hr = m_pDevice->CreateShaderResourceView(pTex->pResource, nullptr, &pTex->pResourceView);
	assert(hr == S_OK);

	return m_textures.getId(pTex);
}

ID3D11Buffer* RdrContext::CreateVertexBuffer(const void* vertices, int size)
{
	return createBuffer(m_pDevice, vertices, size, D3D11_BIND_VERTEX_BUFFER);
}

ID3D11Buffer* RdrContext::CreateIndexBuffer(const void* indices, int size)
{
	return createBuffer(m_pDevice, indices, size, D3D11_BIND_INDEX_BUFFER);
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

static D3D11_COMPARISON_FUNC getComparisonFuncD3d(RdrComparisonFunc cmpFunc)
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
};

ID3D11SamplerState* CreateSamplerState(ID3D11Device* pDevice, bool bPointSample, RdrComparisonFunc cmpFunc)
{
	D3D11_SAMPLER_DESC desc;
	ID3D11SamplerState* pSampler;

	desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	if (cmpFunc != kComparisonFunc_Never)
	{
		desc.Filter = bPointSample ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	}
	else
	{
		desc.Filter = bPointSample ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	}
	desc.ComparisonFunc = getComparisonFuncD3d(cmpFunc);
	desc.MaxAnisotropy = 1;
	desc.MaxLOD = FLT_MAX;
	desc.MinLOD = -FLT_MIN;
	desc.MipLODBias = 0;
	desc.BorderColor[0] = 1.f;
	desc.BorderColor[1] = 1.f;
	desc.BorderColor[2] = 1.f;
	desc.BorderColor[3] = 1.f;

	HRESULT hr = pDevice->CreateSamplerState(&desc, &pSampler);
	assert(hr == S_OK);
	return pSampler;
}

RdrGeoHandle RdrContext::LoadGeo(const char* filename)
{
	GeoMap::iterator iter = m_geoCache.find(filename);
	if (iter != m_geoCache.end())
		return iter->second;


	static const int kReserveSize = 1024;
	std::vector<Vertex> verts;
	std::vector<Vec3> positions;
	std::vector<Vec3> normals;
	std::vector<Vec2> texcoords;
	std::vector<uint16> tris;

	char fullFilename[MAX_PATH];
	sprintf_s(fullFilename, "data/geo/%s", filename);
	std::ifstream file(fullFilename);

	char line[1024];

	while (file.getline(line, 1024))
	{
		char* context = nullptr;
		char* tok = strtok_s(line, " ", &context);

		if (strcmp(tok, "v") == 0)
		{
			// Position
			Vec3 pos;
			pos.x = (float)atof(strtok_s(nullptr, " ", &context));
			pos.y = (float)atof(strtok_s(nullptr, " ", &context));
			pos.z = (float)atof(strtok_s(nullptr, " ", &context));
			positions.push_back(pos);
		}
		else if (strcmp(tok, "vn") == 0)
		{
			// Normal
			Vec3 norm;
			norm.x = (float)atof(strtok_s(nullptr, " ", &context));
			norm.y = (float)atof(strtok_s(nullptr, " ", &context));
			norm.z = (float)atof(strtok_s(nullptr, " ", &context));
			normals.push_back(norm);
		}
		else if (strcmp(tok, "vt") == 0)
		{
			// Tex coord
			Vec2 uv;
			uv.x = (float)atof(strtok_s(nullptr, " ", &context));
			uv.y = (float)atof(strtok_s(nullptr, " ", &context));
			texcoords.push_back(uv);
		}
		else if (strcmp(tok, "f") == 0)
		{
			// Face
			Vec3 tri;
			for (int i = 0; i < 3; ++i)
			{
				int iPos = atoi(strtok_s(nullptr, "/ ", &context)) - 1;
				int iUV = atoi(strtok_s(nullptr, "/ ", &context)) - 1;
				int iNorm = atoi(strtok_s(nullptr, "/ ", &context)) - 1;

				Vertex vert;
				vert.position = positions[iPos];
				vert.texcoord = texcoords[iUV];
				vert.normal = normals[iNorm];

				tris.push_back((uint16)verts.size());
				verts.push_back(vert);
			}
		}
	}

	for (uint i = 0; i < tris.size(); i += 3)
	{
		Vertex& v0 = verts[tris[i+0]];
		Vertex& v1 = verts[tris[i+1]];
		Vertex& v2 = verts[tris[i+2]];

		Vec3 edge1 = v1.position - v0.position;
		Vec3 edge2 = v2.position - v0.position;

		Vec2 uvEdge1 = v1.texcoord - v0.texcoord;
		Vec2 uvEdge2 = v2.texcoord - v0.texcoord;

		float r = 1.f / (uvEdge1.y * uvEdge2.x - uvEdge1.x * uvEdge2.y);

		Vec3 tangent = (edge1 * -uvEdge2.y + edge2 * uvEdge1.y) * r;
		Vec3 bitangent = (edge1 * -uvEdge2.x + edge2 * uvEdge1.x) * r;

		tangent = Vec3Normalize(tangent);
		bitangent = Vec3Normalize(bitangent);

		v0.tangent = v1.tangent = v2.tangent = tangent;
		v0.bitangent = v1.bitangent = v2.bitangent = bitangent;
	}

	RdrGeometry* pGeo = m_geo.alloc();

	pGeo->numVerts = verts.size();
	pGeo->numIndices = tris.size();
	pGeo->pVertexBuffer = CreateVertexBuffer(verts.data(), sizeof(Vertex) * pGeo->numVerts);
	pGeo->pIndexBuffer = CreateIndexBuffer(tris.data(), sizeof(uint16) * pGeo->numIndices);
	pGeo->vertStride = sizeof(Vertex);

	// Calc radius
	Vec3 vMin(FLT_MAX, FLT_MAX, FLT_MAX);
	Vec3 vMax(-FLT_MIN, -FLT_MIN, -FLT_MIN);
	for (int i = 0; i < pGeo->numVerts; ++i)
	{
		vMin.x = min(verts[i].position.x, vMin.x);
		vMax.x = max(verts[i].position.x, vMax.x);
		vMin.y = min(verts[i].position.y, vMin.y);
		vMax.y = max(verts[i].position.y, vMax.y);
		vMin.z = min(verts[i].position.z, vMin.z);
		vMax.z = max(verts[i].position.z, vMax.z);
	}
	pGeo->size = vMax - vMin;
	pGeo->radius = Vec3Length(pGeo->size);

	RdrGeoHandle hGeo = m_geo.getId(pGeo);
	m_geoCache.insert(std::make_pair(filename, hGeo));
	return hGeo;
}

RdrGeoHandle RdrContext::CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size)
{
	RdrGeometry* pGeo = m_geo.alloc();

	pGeo->numVerts = numVerts;
	pGeo->numIndices = numIndices;
	pGeo->pVertexBuffer = CreateVertexBuffer(pVertData, vertStride * pGeo->numVerts);
	pGeo->pIndexBuffer = CreateIndexBuffer(pIndexData, sizeof(uint16) * pGeo->numIndices);
	pGeo->vertStride = vertStride;
	pGeo->size = size;
	pGeo->radius = Vec3Length(size);

	return m_geo.getId(pGeo);
}

RdrTextureHandle RdrContext::LoadTexture(const char* filename, bool bPointSample)
{
	TextureMap::iterator iter = m_textureCache.find(filename);
	if (iter != m_textureCache.end())
		return iter->second; // todo: check sample state

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

	RdrTexture* pTex = m_textures.alloc();

	pTex->pTexture = pTexture;
	pTex->pSamplerState = CreateSamplerState(m_pDevice, bPointSample, kComparisonFunc_Never);
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

	RdrTextureHandle hTex = m_textures.getId(pTex);
	m_textureCache.insert(std::make_pair(filename, hTex));
	return hTex;
}

void RdrContext::ReleaseTexture(RdrTextureHandle hTex)
{
	RdrTexture* pTex = m_textures.get(hTex);
	if (pTex->pResource)
		pTex->pResource->Release();
	if (pTex->pResourceView)
		pTex->pResourceView->Release();
	if (pTex->pUnorderedAccessView)
		pTex->pUnorderedAccessView->Release();
	if (pTex->pSamplerState)
		pTex->pSamplerState->Release();

	m_textures.releaseId(hTex);
}

RdrTextureHandle RdrContext::CreateTexture2D(uint width, uint height, RdrResourceFormat format)
{
	D3D11_TEXTURE2D_DESC desc = { 0 };

	desc.ArraySize = 1;
	desc.CPUAccessFlags = 0;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (resourceFormatIsDepth(format))
		desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	else
		desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.SampleDesc.Count = 1;
	desc.Format = getD3DTypelessFormat(format);
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;

	ID3D11Texture2D* pTexture;
	HRESULT hr = m_pDevice->CreateTexture2D(&desc, nullptr, &pTexture);
	assert(hr == S_OK);

	RdrTexture* pTex = m_textures.alloc();
	pTex->pTexture = pTexture;

	//if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(format);
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Texture2D.MostDetailedMip = 0;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

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

	return m_textures.getId(pTex);
}

RdrTextureHandle RdrContext::CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat format, RdrComparisonFunc cmpFunc)
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

	RdrTexture* pTex = m_textures.alloc();
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

	pTex->pSamplerState = CreateSamplerState(m_pDevice, false, cmpFunc);

	return m_textures.getId(pTex);
}

ShaderHandle RdrContext::LoadVertexShader(const char* filename, D3D11_INPUT_ELEMENT_DESC* aDesc, int numElements)
{
	ShaderMap::iterator iter = m_vertexShaderCache.find(filename);
	if (iter != m_vertexShaderCache.end())
		return iter->second;

	VertexShader* pShader = m_vertexShaders.alloc();

	*pShader = VertexShader::Create(m_pDevice, filename);

	HRESULT hr = m_pDevice->CreateInputLayout(aDesc, numElements,
		pShader->pCompiledData->GetBufferPointer(), pShader->pCompiledData->GetBufferSize(),
		&pShader->pInputLayout);
	assert(hr == S_OK);

	ShaderHandle hShader = m_vertexShaders.getId(pShader);
	m_vertexShaderCache.insert(std::make_pair(filename, hShader));
	return hShader;
}

ShaderHandle RdrContext::LoadPixelShader(const char* filename)
{
	ShaderMap::iterator iter = m_pixelShaderCache.find(filename);
	if (iter != m_pixelShaderCache.end())
		return iter->second;

	PixelShader* pShader = m_pixelShaders.alloc();
	*pShader = PixelShader::Create(m_pDevice, filename);

	ShaderHandle hShader = m_pixelShaders.getId(pShader);
	m_pixelShaderCache.insert(std::make_pair(filename, hShader));
	return hShader;
}

ShaderHandle RdrContext::LoadComputeShader(const char* filename)
{
	ComputeShader* pShader = m_computeShaders.alloc();
	*pShader = ComputeShader::Create(m_pDevice, filename);
	return m_computeShaders.getId(pShader);
}

/////////////////////////
// RdrDrawOp
typedef FreeList<RdrDrawOp, 16 * 1024> RdrDrawOpList;
RdrDrawOpList g_drawOpList;

RdrDrawOp* RdrDrawOp::Allocate()
{
	RdrDrawOp* pDrawOp = g_drawOpList.alloc();
	memset(pDrawOp, 0, sizeof(RdrDrawOp));
	return pDrawOp;
}

void RdrDrawOp::Release(RdrDrawOp* pDrawOp)
{
	g_drawOpList.release(pDrawOp);
}