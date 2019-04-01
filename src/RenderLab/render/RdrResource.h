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
public:
	void SetName(const char* name) { filename = name; }
	void InitAsTexture(const RdrTextureInfo& texInfo, RdrResourceAccessFlags accessFlags);
	void InitAsBuffer(const RdrBufferInfo& bufferInfo, RdrResourceAccessFlags accessFlags);
	void InitAsConstantBuffer(RdrResourceAccessFlags accessFlags, uint nByteSize);
	void InitAsIndexBuffer(RdrResourceAccessFlags accessFlags, uint nByteSize);
	void InitAsVertexBuffer(RdrResourceAccessFlags accessFlags, uint nByteSize);

	void BindDeviceResources(ID3D12Resource* pResource, D3D12_RESOURCE_STATES eInitialState, RdrShaderResourceView* pSRV, RdrUnorderedAccessView* pUAV);
	
	bool IsTexture() const { return m_bIsTexture; }
	bool IsBuffer() const { return !m_bIsTexture; }

	bool IsValid() const { return !!m_pResource; }

	void Reset();

	// donotcheckin - make RdrResource owner of all its data - extract from RdrContext
	D3D12_RESOURCE_STATES GetResourceState() const { return m_eResourceState; }
	void SetResourceState(D3D12_RESOURCE_STATES eState) { m_eResourceState = eState; }

	ID3D12Resource* GetResource() const { MarkUsedThisFrame();  return m_pResource; }
	const RdrShaderResourceView& GetSRV() const { MarkUsedThisFrame(); return m_srv; }
	const RdrUnorderedAccessView GetUAV() const { MarkUsedThisFrame(); return m_uav; }

	uint GetSize() const { return m_size; }
	RdrResourceAccessFlags GetAccessFlags() const { return m_accessFlags; }

	void MarkUsedThisFrame() const;
	uint64 GetLastUsedFrame() const { return m_nLastUsedFrameCode; }

	const RdrBufferInfo& GetBufferInfo() const { return m_bufferInfo; }
	const RdrTextureInfo& GetTextureInfo() const { return m_textureInfo; }
private:
	ID3D12Resource* m_pResource;
	RdrShaderResourceView m_srv;
	RdrUnorderedAccessView m_uav;
	D3D12_RESOURCE_STATES m_eResourceState;
	RdrResourceAccessFlags m_accessFlags;
	uint m_size;
	uint64 m_nLastUsedFrameCode;

	const char* filename;

	union
	{
		RdrTextureInfo m_textureInfo;
		RdrBufferInfo m_bufferInfo;
	};

	bool m_bIsTexture; // Whether this resource is a texture or a buffer.
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
