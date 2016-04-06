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

typedef uint16 RdrResourceHandle;
struct RdrResource
{
	union
	{
		ID3D11Texture2D* pTexture;
		ID3D11Buffer*    pBuffer;
		ID3D11Resource*  pResource;
	};

	ID3D11ShaderResourceView*	pResourceView;
	ID3D11UnorderedAccessView*	pUnorderedAccessView;

	union
	{
		RdrTextureInfo texInfo;
		RdrStructuredBufferInfo bufferInfo;
	};
};
