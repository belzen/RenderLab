#pragma once

#include "RdrDeviceTypes.h"
#include "MathLib\Vec4.h"
#include "FreeList.h"

interface ID3D12Resource;

enum class RdrTextureType
{
	k2D,
	k3D,
	kCube
};

struct RdrTextureInfo
{
	const char* filename;

	RdrTextureType texType;
	RdrResourceFormat format;
	uint width;
	uint height;
	uint depth; // Array size for 2d/cube.  Depth for 3d.
	uint mipLevels;
	uint sampleCount;
};

enum class RdrBufferType
{
	Data,
	Structured,
	Vertex
};

struct RdrBufferInfo
{
	RdrBufferType eType;
	RdrResourceFormat eFormat;
	uint numElements;
	uint elementSize;
};

struct RdrShaderResourceView
{
	D3D12DescriptorHandle hView;
	RdrResource* pResource;
};

struct RdrUnorderedAccessView
{
	D3D12DescriptorHandle hView;
	RdrResource* pResource;
};

struct RdrResource
{
	ID3D12Resource* pResource;
	RdrShaderResourceView srv;
	RdrUnorderedAccessView uav;
	D3D12_RESOURCE_STATES eResourceState;
	RdrResourceAccessFlags accessFlags;
	uint size;

	union
	{
		RdrTextureInfo texInfo;
		RdrBufferInfo bufferInfo;
	};

	bool bIsTexture; // Whether this resource is a texture or a buffer.
};

//////////////////////////////////////////////////////////////////////////

inline bool operator == (const RdrShaderResourceView& rLeft, const RdrShaderResourceView& rRight)
{
	return rLeft.hView == rRight.hView;
}

inline bool operator != (const RdrShaderResourceView& rLeft, const RdrShaderResourceView& rRight)
{
	return !(rLeft == rRight);
}

//////////////////////////////////////////////////////////////////////////

typedef FreeList<RdrResource, 1024> RdrResourceList;
typedef RdrResourceList::Handle RdrResourceHandle;

typedef FreeList<RdrResource, 16 * 1024> RdrConstantBufferList; // donotcheckin - even bother with this lists anymore?
typedef RdrConstantBufferList::Handle RdrConstantBufferHandle;

typedef FreeList<RdrRenderTargetView, 128> RdrRenderTargetViewList;
typedef RdrRenderTargetViewList::Handle RdrRenderTargetViewHandle;

typedef FreeList<RdrDepthStencilView, 128> RdrDepthStencilViewList;
typedef RdrDepthStencilViewList::Handle RdrDepthStencilViewHandle;

typedef FreeList<RdrShaderResourceView, 1024> RdrShaderResourceViewList;
typedef RdrShaderResourceViewList::Handle RdrShaderResourceViewHandle;
