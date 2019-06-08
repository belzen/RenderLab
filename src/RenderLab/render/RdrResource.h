#pragma once

#include "RdrDeviceTypes.h"
#include "MathLib\Vec4.h"
#include "FreeList.h"
#include "RdrDebugBackpointer.h"

interface ID3D12Resource;
class RdrContext;

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
	Vertex,
	Index
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
	void Reset() 
	{ pDesc = nullptr; pResource = nullptr; }

	RdrDescriptors* pDesc;
	const RdrResource* pResource;
};

struct RdrUnorderedAccessView
{
	void Reset() { pDesc = nullptr; pResource = nullptr; }

	RdrDescriptors* pDesc;
	const RdrResource* pResource;
};

struct RdrConstantBufferView
{
	void Reset() { pDesc = nullptr; pResource = nullptr; }

	RdrDescriptors* pDesc;
	const RdrResource* pResource;
};

struct RdrResource
{
public:
	void SetName(const char* name) { filename = name; }

	/////////////////////////////////////////////////////////////
	// Resources
	bool CreateTexture(RdrContext& context, const RdrTextureInfo& rTexInfo, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	bool CreateBuffer(RdrContext& context, const RdrBufferInfo& rBufferInfo, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	bool CreateStructuredBuffer(RdrContext& context, int numElements, int elementSize, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	void CopyResourceRegion(RdrContext& context, const RdrBox& srcRegion, const RdrResource& rDstResource, const IVec3& dstOffset) const;
	void ReadResource(RdrContext& context, void* pDstData, uint dstDataSize) const;

	void ReleaseResource(RdrContext& context);

	void ResolveResource(RdrContext& context, const RdrResource& rDst) const;

	RdrShaderResourceView CreateShaderResourceViewTexture(RdrContext& context);
	RdrShaderResourceView CreateShaderResourceViewBuffer(RdrContext& context, uint firstElement);
	void ReleaseShaderResourceView(RdrContext& context, RdrShaderResourceView& resourceView);

	/////////////////////////////////////////////////////////////
	// Constant Buffers
	bool CreateConstantBuffer(RdrContext& context, uint size, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	void UpdateResource(RdrContext& context, const void* pData, uint dataSize);

	void TransitionState(RdrContext& context, D3D12_RESOURCE_STATES eState);

	bool IsTexture() const { return m_bIsTexture; }
	bool IsBuffer() const { return !m_bIsTexture; }

	bool IsValid() const { return !!m_pResource; }

	ID3D12Resource* GetResource() const { MarkUsedThisFrame();  return m_pResource; }
	const RdrShaderResourceView& GetSRV() const { MarkUsedThisFrame(); return m_srv; }
	const RdrUnorderedAccessView GetUAV() const { MarkUsedThisFrame(); return m_uav; }
	const RdrConstantBufferView& GetCBV() const { MarkUsedThisFrame(); return m_cbv; }

	uint GetSize() const { return m_size; }
	RdrResourceAccessFlags GetAccessFlags() const { return m_accessFlags; }

	void MarkUsedThisFrame() const;
	uint64 GetLastUsedFrame() const { return m_nLastUsedFrameCode; }

	const RdrBufferInfo& GetBufferInfo() const { return m_bufferInfo; }
	const RdrTextureInfo& GetTextureInfo() const { return m_textureInfo; }
private:
	ID3D12Resource* CreateBufferCommon(RdrContext& context, const int size, RdrResourceAccessFlags accessFlags, const D3D12_RESOURCE_STATES initialState);


	ID3D12Resource* m_pResource;
	ID3D12Resource* m_pUploadResource;

	RdrShaderResourceView m_srv;
	RdrUnorderedAccessView m_uav;
	RdrConstantBufferView m_cbv;
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

	RdrDebugBackpointer m_debugCreator;
};

//////////////////////////////////////////////////////////////////////////

inline bool operator == (const RdrShaderResourceView& rLeft, const RdrShaderResourceView& rRight)
{
	return rLeft.pDesc == rRight.pDesc;
}

inline bool operator != (const RdrShaderResourceView& rLeft, const RdrShaderResourceView& rRight)
{
	return !(rLeft == rRight);
}

//////////////////////////////////////////////////////////////////////////

typedef FreeList<RdrResource, 1024> RdrResourceList;
typedef RdrResourceList::Handle RdrResourceHandle;

typedef FreeList<RdrResource, 16 * 1024> RdrConstantBufferList;
typedef RdrConstantBufferList::Handle RdrConstantBufferHandle;

typedef FreeList<RdrRenderTargetView, 128> RdrRenderTargetViewList;
typedef RdrRenderTargetViewList::Handle RdrRenderTargetViewHandle;

typedef FreeList<RdrDepthStencilView, 128> RdrDepthStencilViewList;
typedef RdrDepthStencilViewList::Handle RdrDepthStencilViewHandle;

typedef FreeList<RdrShaderResourceView, 1024> RdrShaderResourceViewList;
typedef RdrShaderResourceViewList::Handle RdrShaderResourceViewHandle;

