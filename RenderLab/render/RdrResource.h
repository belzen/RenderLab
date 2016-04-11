#pragma once

#include "RdrDeviceTypes.h"

struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11Resource;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

struct RdrTextureInfo
{
	const char* filename;

	RdrResourceFormat format;
	uint width;
	uint height;
	uint mipLevels;
	uint arraySize;
	uint sampleCount;
	bool bCubemap;
};

struct RdrStructuredBufferInfo
{
	uint elementSize;
	uint numElements;
};

struct RdrShaderResourceView
{
	union
	{
		ID3D11ShaderResourceView* pViewD3D11;
		void* pTypeless;
	};
};

struct RdrUnorderedAccessView
{
	union
	{
		ID3D11UnorderedAccessView* pViewD3D11;
		void* pTypeless;
	};
};

typedef uint16 RdrResourceHandle;
struct RdrResource
{
	union
	{
		ID3D11Texture2D* pTexture;
		ID3D11Buffer*    pBuffer;
		ID3D11Resource*  pResource;
	};

	RdrShaderResourceView resourceView;
	RdrUnorderedAccessView uav;

	union
	{
		RdrTextureInfo texInfo;
		RdrStructuredBufferInfo bufferInfo;
	};
};

union RdrConstantBufferDeviceObj
{
	ID3D11Buffer* pBufferD3D11;
};

typedef uint16 RdrConstantBufferHandle;
struct RdrConstantBuffer
{
	static inline uint GetRequiredSize(uint size) { return sizeof(Vec4) * (uint)ceilf(size / (float)sizeof(Vec4)); }

	RdrUnorderedAccessView uav;
	RdrConstantBufferDeviceObj bufferObj;
	uint size;
};