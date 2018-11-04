#pragma once

#include "RdrDeviceTypes.h"
#include "MathLib\Vec4.h"
#include "FreeList.h"

struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11Texture3D;
struct ID3D11Resource;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

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
	union
	{
		ID3D11ShaderResourceView* pViewD3D11;
		D3D12DescriptorHandle hViewD3D12;
		void* pTypeless;
	};
};

struct RdrUnorderedAccessView
{
	union
	{
		ID3D11UnorderedAccessView* pViewD3D11;
		D3D12DescriptorHandle hViewD3D12;
		void* pTypeless;
	};
};

struct RdrResource
{
	union
	{
		ID3D11Texture2D* pTexture2d;
		ID3D11Texture3D* pTexture3d;
		ID3D11Buffer*    pBuffer;
		ID3D11Resource*  pResource;
		ID3D12Resource*  pResource12;
	};

	RdrShaderResourceView resourceView;
	RdrUnorderedAccessView uav;
	RdrResourceUsage eUsage;

	union
	{
		RdrTextureInfo texInfo;
		RdrBufferInfo bufferInfo;
	};

	bool bIsTexture; // Whether this resource is a texture or a buffer.
};

union RdrConstantBufferDeviceObj
{
	ID3D11Buffer* pBufferD3D11;
	ID3D12Resource* pBufferD3D12;
	void* pTypeless;
};

struct RdrConstantBuffer
{
	RdrUnorderedAccessView uav;
	RdrConstantBufferDeviceObj bufferObj;
	RdrCpuAccessFlags cpuAccessFlags;
	uint size;
};

//////////////////////////////////////////////////////////////////////////

inline bool operator == (const RdrConstantBufferDeviceObj& rLeft, const RdrConstantBufferDeviceObj& rRight)
{
	return rLeft.pTypeless == rRight.pTypeless;
}

inline bool operator != (const RdrConstantBufferDeviceObj& rLeft, const RdrConstantBufferDeviceObj& rRight)
{
	return !(rLeft == rRight);
}

inline bool operator == (const RdrShaderResourceView& rLeft, const RdrShaderResourceView& rRight)
{
	return rLeft.pTypeless == rRight.pTypeless;
}

inline bool operator != (const RdrShaderResourceView& rLeft, const RdrShaderResourceView& rRight)
{
	return !(rLeft == rRight);
}

//////////////////////////////////////////////////////////////////////////

typedef FreeList<RdrResource, 1024> RdrResourceList;
typedef RdrResourceList::Handle RdrResourceHandle;

typedef FreeList<RdrConstantBuffer, 16 * 1024> RdrConstantBufferList;
typedef RdrConstantBufferList::Handle RdrConstantBufferHandle;

typedef FreeList<RdrRenderTargetView, 128> RdrRenderTargetViewList;
typedef RdrRenderTargetViewList::Handle RdrRenderTargetViewHandle;

typedef FreeList<RdrDepthStencilView, 128> RdrDepthStencilViewList;
typedef RdrDepthStencilViewList::Handle RdrDepthStencilViewHandle;

typedef FreeList<RdrShaderResourceView, 1024> RdrShaderResourceViewList;
typedef RdrShaderResourceViewList::Handle RdrShaderResourceViewHandle;
