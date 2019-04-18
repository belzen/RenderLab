#include "Precompiled.h"
#include "RdrResource.h"
#include "RdrContext.h"
#include "Renderer.h"

namespace
{
	bool resourceFormatIsDepth(const RdrResourceFormat eFormat)
	{
		return eFormat == RdrResourceFormat::D16 || eFormat == RdrResourceFormat::D24_UNORM_S8_UINT;
	}

	bool resourceFormatIsCompressed(const RdrResourceFormat eFormat)
	{
		return eFormat == RdrResourceFormat::DXT5 || eFormat == RdrResourceFormat::DXT1;
	}

	CD3DX12_HEAP_PROPERTIES selectHeapProperties(const RdrResourceAccessFlags accessFlags)
	{
		if (IsFlagSet(accessFlags, RdrResourceAccessFlags::CpuWrite))
			return CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		else if (accessFlags == RdrResourceAccessFlags::CpuRO_GpuRW)
			return CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

		return CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	}

	void createBufferSrv(ID3D12Device* pDevice, DescriptorHeap& srvHeap, ID3D12Resource* pResource, uint numElements, uint structureByteStride, RdrResourceFormat eFormat, int firstElement, D3D12DescriptorHandle* pOutView)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Buffer.FirstElement = firstElement;
		desc.Buffer.NumElements = numElements;
		desc.Buffer.StructureByteStride = structureByteStride;
		desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		desc.Format = getD3DFormat(eFormat);
		desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle = srvHeap.AllocateDescriptor();
		pDevice->CreateShaderResourceView(pResource, &desc, descHandle);

		*pOutView = descHandle;
	}

	bool createTextureSrv(ID3D12Device* pDevice, DescriptorHeap& srvHeap, const RdrTextureInfo& rTexInfo, ID3D12Resource* pResource, D3D12DescriptorHandle* pOutView)
	{
		bool bIsMultisampled = (rTexInfo.sampleCount > 1);
		bool bIsArray = (rTexInfo.depth > 1);

		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.Format = getD3DFormat(rTexInfo.format);
		if (rTexInfo.texType == RdrTextureType::k3D)
		{
			viewDesc.Texture3D.MipLevels = rTexInfo.mipLevels;
			viewDesc.Texture3D.MostDetailedMip = 0;
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		}
		else if (rTexInfo.texType == RdrTextureType::kCube)
		{
			if (rTexInfo.depth > 6)
			{
				viewDesc.TextureCubeArray.First2DArrayFace = 0;
				viewDesc.TextureCubeArray.MipLevels = rTexInfo.mipLevels;
				viewDesc.TextureCubeArray.MostDetailedMip = 0;
				viewDesc.TextureCubeArray.NumCubes = rTexInfo.depth / 6;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
			}
			else
			{
				viewDesc.TextureCube.MipLevels = rTexInfo.mipLevels;
				viewDesc.TextureCube.MostDetailedMip = 0;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			}
		}
		else if (bIsArray)
		{
			if (bIsMultisampled)
			{
				viewDesc.Texture2DMSArray.ArraySize = rTexInfo.depth;
				viewDesc.Texture2DMSArray.FirstArraySlice = 0;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
			}
			else
			{
				viewDesc.Texture2DArray.ArraySize = rTexInfo.depth;
				viewDesc.Texture2DArray.FirstArraySlice = 0;
				viewDesc.Texture2DArray.MipLevels = rTexInfo.mipLevels;
				viewDesc.Texture2DArray.MostDetailedMip = 0;
				viewDesc.Texture2DArray.PlaneSlice = 0;
				viewDesc.Texture2DArray.ResourceMinLODClamp = 0.f;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			}
		}
		else
		{
			if (bIsMultisampled)
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
				viewDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
			}
			else
			{
				viewDesc.Texture2D.MipLevels = rTexInfo.mipLevels;
				viewDesc.Texture2D.MostDetailedMip = 0;
				viewDesc.Texture2D.PlaneSlice = 0;
				viewDesc.Texture2D.ResourceMinLODClamp = 0.f;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			}
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle = srvHeap.AllocateDescriptor();
		pDevice->CreateShaderResourceView(pResource, &viewDesc, descHandle);

		*pOutView = descHandle;
		return true;
	}


	ID3D12Resource* createUploadResource(ID3D12Device* pDevice, uint nByteSize)
	{
		ID3D12Resource* pUploadResource;

		CD3DX12_HEAP_PROPERTIES heapProperties = selectHeapProperties(RdrResourceAccessFlags::CpuRW_GpuRO);
		HRESULT hr = pDevice->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(nByteSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&pUploadResource));

		ValidateHResult(hr, __FUNCTION__, "Failed to create upload buffer!");

		return pUploadResource;
	}
}

void RdrResource::MarkUsedThisFrame() const
{
	const_cast<RdrResource*>(this)->m_nLastUsedFrameCode = g_pRenderer->GetContext()->GetFrameNum();
}

bool RdrResource::CreateTexture(RdrContext& context, const RdrTextureInfo& rTexInfo, RdrResourceAccessFlags accessFlags, const void* pSrcData)
{
	ID3D12Device* pDevice = context.GetDevice();
	DescriptorHeap& srvHeap = context.GetSrvHeap();

	m_eResourceState = D3D12_RESOURCE_STATE_COMMON;
	m_textureInfo = rTexInfo;
	m_accessFlags = accessFlags;
	m_bIsTexture = true;

	bool bIsMultisampled = (m_textureInfo.sampleCount > 1);
	bool bIsArray = (m_textureInfo.depth > 1);

	D3D12_RESOURCE_DESC desc;
	desc.Alignment = 0;
	desc.Width = m_textureInfo.width;
	desc.Height = m_textureInfo.height;
	desc.DepthOrArraySize = m_textureInfo.depth;
	desc.MipLevels = m_textureInfo.mipLevels;
	desc.Format = getD3DTypelessFormat(m_textureInfo.format);
	desc.SampleDesc.Count = m_textureInfo.sampleCount;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	switch (m_textureInfo.texType)
	{
	case RdrTextureType::k2D:
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		break;
	case RdrTextureType::kCube:
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.DepthOrArraySize = m_textureInfo.depth * 6;
		break;
	case RdrTextureType::k3D:
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		break;
	default:
		assert(false);
		return false;
	}

	D3D12_CLEAR_VALUE clearValue;
	D3D12_CLEAR_VALUE* pClearValue = nullptr;
	if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuRenderTarget))
	{
		if (resourceFormatIsDepth(m_textureInfo.format))
		{
			pClearValue = &clearValue;
			clearValue.Format = getD3DDepthFormat(m_textureInfo.format);
			clearValue.DepthStencil.Depth = 1.f;
			clearValue.DepthStencil.Stencil = 0;

			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		}
		else
		{
			pClearValue = &clearValue;
			clearValue.Format = getD3DFormat(m_textureInfo.format);
			clearValue.Color[0] = 0.f;
			clearValue.Color[1] = 0.f;
			clearValue.Color[2] = 0.f;
			clearValue.Color[3] = 0.f;

			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}
	}
	else if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
	{
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	D3D12_HEAP_PROPERTIES heapProps = selectHeapProperties(accessFlags);

	HRESULT hr = pDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		m_eResourceState,
		pClearValue,
		IID_PPV_ARGS(&m_pResource));

	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create texture!"))
		return false;

	if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuRead))
	{
		createTextureSrv(pDevice, srvHeap, m_textureInfo, m_pResource, &m_srv.hView);
		m_srv.pResource = this;
	}

	if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(m_textureInfo.format);
		if (m_textureInfo.texType == RdrTextureType::k3D)
		{

			viewDesc.Texture3D.MipSlice = 0;
			viewDesc.Texture3D.FirstWSlice = 0;
			viewDesc.Texture3D.WSize = m_textureInfo.depth;
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		}
		else if (bIsArray)
		{
			viewDesc.Texture2DArray.ArraySize = m_textureInfo.depth;
			viewDesc.Texture2DArray.FirstArraySlice = 0;
			viewDesc.Texture2DArray.MipSlice = 0;
			viewDesc.Texture2DArray.PlaneSlice = 0;
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		}
		else
		{
			viewDesc.Texture2D.MipSlice = 0;
			viewDesc.Texture2D.PlaneSlice = 0;
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		}

		m_uav.hView = srvHeap.AllocateDescriptor();
		m_uav.pResource = this;
		pDevice->CreateUnorderedAccessView(m_pResource, nullptr, &viewDesc, m_uav.hView);

		assert(hr == S_OK);
	}

	//////////////////////////////////////////////////////////////////////////

	if (pSrcData)
	{
		UpdateResource(context, pSrcData, 0);
	}

	return true;
}

void RdrResource::ReleaseResource(RdrContext& context)
{
	DescriptorHeap& srvHeap = context.GetSrvHeap();

	if (m_pResource)
	{
		m_pResource->Release();
		m_pResource = nullptr;
	}
	if (m_pUploadResource)
	{
		m_pUploadResource->Release();
		m_pUploadResource = nullptr;
	}
	if (m_srv.hView.ptr)
	{
		srvHeap.FreeDescriptor(m_srv.hView);
		m_srv.hView.ptr = 0;
	}
	if (m_uav.hView.ptr)
	{
		srvHeap.FreeDescriptor(m_uav.hView);
		m_uav.hView.ptr = 0;
	}
}

ID3D12Resource* RdrResource::CreateBufferCommon(RdrContext& context, const int size, RdrResourceAccessFlags accessFlags, const D3D12_RESOURCE_STATES initialState)
{
	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = size;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	CD3DX12_HEAP_PROPERTIES heapProperties = selectHeapProperties(accessFlags);
	ID3D12Resource* pResource = nullptr;

	HRESULT hr = context.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&pResource));

	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create resource!"))
		return nullptr;

	return pResource;
}

bool RdrResource::CreateStructuredBuffer(RdrContext& context, int numElements, int elementSize, RdrResourceAccessFlags accessFlags, const void* pSrcData)
{
	RdrBufferInfo info;
	info.eType = RdrBufferType::Structured;
	info.elementSize = elementSize;
	info.eFormat = RdrResourceFormat::UNKNOWN;
	info.numElements = numElements;

	return CreateBuffer(context, info, accessFlags, pSrcData);
}

bool RdrResource::CreateBuffer(RdrContext& context, const RdrBufferInfo& rBufferInfo, RdrResourceAccessFlags accessFlags, const void* pSrcData)
{
	ID3D12Device* pDevice = context.GetDevice();
	DescriptorHeap& srvHeap = context.GetSrvHeap();

	uint nDataSize = 0;
	uint nElementSize = 0;
	switch(rBufferInfo.eType)
	{
	case RdrBufferType::Data:
		m_eResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
		nDataSize = rBufferInfo.numElements * rdrGetTexturePitch(1, rBufferInfo.eFormat);
		break;
	case RdrBufferType::Structured:
		m_eResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
		nDataSize = rBufferInfo.numElements * rBufferInfo.elementSize;
		nElementSize = rBufferInfo.elementSize;
		assert(rBufferInfo.eFormat == RdrResourceFormat::UNKNOWN);
		break;
	case RdrBufferType::Vertex:
		m_eResourceState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		nDataSize = rBufferInfo.numElements * rBufferInfo.elementSize;
		break;
	case RdrBufferType::Index:
		m_eResourceState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
		nDataSize = rBufferInfo.numElements * rdrGetTexturePitch(1, rBufferInfo.eFormat);
		break;
	}

	m_bufferInfo = rBufferInfo;
	m_accessFlags = accessFlags;
	m_bIsTexture = false;
	m_size = nDataSize;

	m_pResource = CreateBufferCommon(context, nDataSize, accessFlags, m_eResourceState);
	if (!m_pResource)
		return false;

	if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuRead))
	{
		createBufferSrv(pDevice, srvHeap, m_pResource, rBufferInfo.numElements, nElementSize, rBufferInfo.eFormat, 0, &m_srv.hView);
		m_srv.pResource = this;
	}

	if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
		desc.Buffer.FirstElement = 0;
		desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		desc.Buffer.NumElements = rBufferInfo.numElements;
		desc.Buffer.StructureByteStride = nElementSize;
		desc.Buffer.CounterOffsetInBytes = 0;
		desc.Format = getD3DFormat(rBufferInfo.eFormat);
		desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		m_uav.hView = srvHeap.AllocateDescriptor();
		m_uav.pResource = this;
		pDevice->CreateUnorderedAccessView(m_pResource, nullptr, &desc, m_uav.hView);
	}

	if (pSrcData)
	{
		UpdateResource(context, pSrcData, nDataSize);
	}

	return true;
}

bool RdrResource::CreateConstantBuffer(RdrContext& context, const void* pData, uint size, RdrResourceAccessFlags accessFlags)
{
	ID3D12Device* pDevice = context.GetDevice();
	DescriptorHeap& srvHeap = context.GetSrvHeap();

	size = (size + 255) & ~255;	// CB size is required to be 256-byte aligned.

	m_eResourceState = IsFlagSet(accessFlags, RdrResourceAccessFlags::CpuWrite) ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	m_pResource = CreateBufferCommon(context, size, accessFlags, m_eResourceState);
	if (!m_pResource)
		return false;

	// Describe and create a constant buffer view.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_pResource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = size;

	m_srv.hView = srvHeap.AllocateDescriptor();
	m_srv.pResource = this;
	pDevice->CreateConstantBufferView(&cbvDesc, m_srv.hView);

	m_accessFlags = accessFlags;
	m_bIsTexture = false;
	m_size = size;

	if (pData)
	{
		UpdateResource(context, pData, size);
	}

	return true;
}

void RdrResource::UpdateResource(RdrContext& context, const void* pSrcData, const uint dataSize)
{
	if (IsFlagSet(m_accessFlags, RdrResourceAccessFlags::CpuWrite))
	{
		void* pDstData;
		CD3DX12_RANGE readRange(0, 0);
		HRESULT hr = m_pResource->Map(0, &readRange, &pDstData);

		if (!ValidateHResult(hr, __FUNCTION__, "Failed to map resource!"))
			return;

		memcpy(pDstData, pSrcData, dataSize);

		CD3DX12_RANGE writeRange(0, dataSize);
		m_pResource->Unmap(0, &writeRange);
	}
	else
	{
		static constexpr uint kMaxSubresources = 16;
		D3D12_SUBRESOURCE_DATA subresourceData[kMaxSubresources];
		uint numSubresources = 0;

		if (m_bIsTexture)
		{
			uint sliceCount = (m_textureInfo.texType == RdrTextureType::kCube) ? m_textureInfo.depth * 6 : m_textureInfo.depth;
			uint resIndex = 0;

			numSubresources = sliceCount * m_textureInfo.mipLevels;

			char* pPos = (char*)pSrcData;
			for (uint slice = 0; slice < sliceCount; ++slice)
			{
				uint mipWidth = m_textureInfo.width;
				uint mipHeight = m_textureInfo.height;
				for (uint mip = 0; mip < m_textureInfo.mipLevels; ++mip)
				{
					int rows = rdrGetTextureRows(mipHeight, m_textureInfo.format);

					subresourceData[resIndex].pData = pPos;
					subresourceData[resIndex].RowPitch = rdrGetTexturePitch(mipWidth, m_textureInfo.format);
					subresourceData[resIndex].SlicePitch = subresourceData[resIndex].RowPitch * rows;

					pPos += subresourceData[resIndex].SlicePitch;

					if (mipWidth > 1)
						mipWidth >>= 1;
					if (mipHeight > 1)
						mipHeight >>= 1;
					++resIndex;
				}
			}
		}
		else
		{
			numSubresources = 1;
			subresourceData[0].pData = pSrcData;
			subresourceData[0].RowPitch = m_size;
			subresourceData[0].SlicePitch = subresourceData[0].RowPitch;
		}

		if (!m_pUploadResource)
		{
			uint uploadBufferSize = (uint)GetRequiredIntermediateSize(m_pResource, 0, numSubresources);
			m_pUploadResource = createUploadResource(context.GetDevice(), uploadBufferSize);
		}

		assert(numSubresources < kMaxSubresources);

		D3D12_RESOURCE_STATES eInitialState = m_eResourceState;
		TransitionState(context, D3D12_RESOURCE_STATE_COPY_DEST);
		UpdateSubresources<16>(context.GetCommandList(), m_pResource, m_pUploadResource, 0, 0, numSubresources, subresourceData);
		TransitionState(context, eInitialState);
	}
}

void RdrResource::CopyResourceRegion(RdrContext& context, const RdrBox& srcRegion, const RdrResource& rDstResource, const IVec3& dstOffset) const
{
	if (IsTexture())
	{
		D3D12_BOX box;
		box.left = srcRegion.left;
		box.right = srcRegion.left + srcRegion.width;
		box.top = srcRegion.top;
		box.bottom = srcRegion.top + srcRegion.height;
		box.front = srcRegion.front;
		box.back = srcRegion.front + srcRegion.depth;

		CD3DX12_TEXTURE_COPY_LOCATION src(m_pResource, 0);
		CD3DX12_TEXTURE_COPY_LOCATION dst(rDstResource.GetResource(), 0);

		context.GetCommandList()->CopyTextureRegion(&dst, dstOffset.x, dstOffset.y, dstOffset.z, &src, &box);
	}
	else
	{
		context.GetCommandList()->CopyBufferRegion(rDstResource.GetResource(), dstOffset.x, m_pResource, srcRegion.left, srcRegion.width);
	}
}

void RdrResource::ReadResource(RdrContext& context, void* pDstData, uint dstDataSize) const
{
	void* pMappedData;
	D3D12_RANGE readRange = { 0, dstDataSize };
	HRESULT hr = m_pResource->Map(0, &readRange, &pMappedData);

	if (!ValidateHResult(hr, __FUNCTION__, "Failed to map resource!"))
		return;

	memcpy(pDstData, pMappedData, dstDataSize);

	D3D12_RANGE writeRange = { 0,0 };
	m_pResource->Unmap(0, nullptr);
}

void RdrResource::ResolveResource(RdrContext& context, const RdrResource& rDst) const
{
	context.GetCommandList()->ResolveSubresource(rDst.GetResource(), 0, m_pResource, 0, getD3DFormat(m_textureInfo.format));
}

void RdrResource::TransitionState(RdrContext& context, D3D12_RESOURCE_STATES eState)
{
	if (m_eResourceState == eState)
		return;

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pResource,
		m_eResourceState,
		eState);

	context.GetCommandList()->ResourceBarrier(1, &barrier);

	m_eResourceState = eState;
}


RdrShaderResourceView RdrResource::CreateShaderResourceViewTexture(RdrContext& context)
{
	ID3D12Device* pDevice = context.GetDevice();
	DescriptorHeap& srvHeap = context.GetSrvHeap();

	RdrShaderResourceView view;
	createTextureSrv(pDevice, srvHeap, m_textureInfo, m_pResource, &view.hView);
	view.pResource = this;
	return view;
}

RdrShaderResourceView RdrResource::CreateShaderResourceViewBuffer(RdrContext& context, uint firstElement)
{
	ID3D12Device* pDevice = context.GetDevice();
	DescriptorHeap& srvHeap = context.GetSrvHeap();

	RdrShaderResourceView view;
	createBufferSrv(pDevice, srvHeap, m_pResource, m_bufferInfo.numElements - firstElement, 0, m_bufferInfo.eFormat, firstElement, &view.hView);
	view.pResource = this;

	return view;
}

void RdrResource::ReleaseShaderResourceView(RdrContext& context, RdrShaderResourceView& resourceView)
{
	DescriptorHeap& srvHeap = context.GetSrvHeap();
	srvHeap.FreeDescriptor(resourceView.hView);
	resourceView.hView.ptr = 0;
}
