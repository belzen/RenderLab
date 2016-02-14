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

static ID3D11Buffer* CreateBuffer(ID3D11Device* pDevice, const void* pSrcData, int size, uint bindFlags)
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
	return CreateBuffer(m_pDevice, vertices, size, D3D11_BIND_VERTEX_BUFFER);
}

ID3D11Buffer* RdrContext::CreateIndexBuffer(const void* indices, int size)
{
	return CreateBuffer(m_pDevice, indices, size, D3D11_BIND_INDEX_BUFFER);
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

static ID3D11ShaderResourceView* CreateShaderResourceView(ID3D11Device* pDevice, ID3D11Resource* pResource)
{
	ID3D11ShaderResourceView* pResourceView;
	HRESULT hr = pDevice->CreateShaderResourceView(pResource, NULL, &pResourceView);
	assert(hr == S_OK);
	return pResourceView;
}

static ID3D11UnorderedAccessView* CreateUnorderedAccessView(ID3D11Device* pDevice, ID3D11Resource* pResource)
{
	ID3D11UnorderedAccessView* pView;
	HRESULT hr = pDevice->CreateUnorderedAccessView(pResource, NULL, &pView);
	assert(hr == S_OK);
	return pView;
}

ID3D11SamplerState* CreateSamplerState(ID3D11Device* pDevice, bool bPointSample)
{
	D3D11_SAMPLER_DESC desc;
	ID3D11SamplerState* pSampler;

	desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.Filter = bPointSample ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
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

	std::ifstream file(filename);

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
	pGeo->radius = Vec3Length(vMax - vMin);

	RdrGeoHandle hGeo = m_geo.getId(pGeo);
	m_geoCache.insert(std::make_pair(filename, hGeo));
	return hGeo;
}

RdrGeoHandle RdrContext::CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices)
{
	RdrGeometry* pGeo = m_geo.alloc();

	pGeo->numVerts = numVerts;
	pGeo->numIndices = numIndices;
	pGeo->pVertexBuffer = CreateVertexBuffer(pVertData, vertStride * pGeo->numVerts);
	pGeo->pIndexBuffer = CreateIndexBuffer(pIndexData, sizeof(uint16) * pGeo->numIndices);
	pGeo->vertStride = vertStride;

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

	FILE* file;
	fopen_s(&file, filename, "rb");

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
	pTex->pResourceView = CreateShaderResourceView(m_pDevice, pTexture);
	pTex->pSamplerState = CreateSamplerState(m_pDevice, bPointSample);

	RdrTextureHandle hTex = m_textures.getId(pTex);
	m_textureCache.insert(std::make_pair(filename, hTex));
	return hTex;
}

RdrTextureHandle RdrContext::CreateTexture2D(uint width, uint height, DXGI_FORMAT format)
{
	D3D11_TEXTURE2D_DESC desc = { 0 };

	desc.ArraySize = 1;
	desc.CPUAccessFlags = 0;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.SampleDesc.Count = 1;
	desc.Format = format;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;

	ID3D11Texture2D* pTexture;
	HRESULT hr = m_pDevice->CreateTexture2D(&desc, nullptr, &pTexture);
	assert(hr == S_OK);

	RdrTexture* pTex = m_textures.alloc();
	pTex->pTexture = pTexture;
	pTex->pResourceView = CreateShaderResourceView(m_pDevice, pTexture);
	pTex->pUnorderedAccessView = CreateUnorderedAccessView(m_pDevice, pTexture);
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