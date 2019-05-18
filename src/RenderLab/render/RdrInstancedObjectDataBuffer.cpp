#include "Precompiled.h"
#include "RdrInstancedObjectDataBuffer.h"
#include "RdrResourceSystem.h"
#include "Renderer.h"

namespace
{
	struct ObjectBuffer
	{
		ObjectBuffer() : currentFrame(0) {}

		FreeList<VsPerObject, 6 * 1024> data;
		RdrResourceHandle hResource;
		uint currentFrame;
	} s_objectBuffer;
}

RdrInstancedObjectDataId RdrInstancedObjectDataBuffer::AllocEntry()
{
	VsPerObject* pData = s_objectBuffer.data.allocSafe();
	return s_objectBuffer.data.getId(pData);
}

VsPerObject* RdrInstancedObjectDataBuffer::GetEntry(RdrInstancedObjectDataId id)
{
	return s_objectBuffer.data.get(id);
}

void RdrInstancedObjectDataBuffer::ReleaseEntry(RdrInstancedObjectDataId id)
{
	s_objectBuffer.data.releaseIdSafe(id);
}

void RdrInstancedObjectDataBuffer::UpdateBuffer(Renderer& rRenderer)
{
	// TODO: This is not safe.  The buffer data could be changing on the main thread
	//	while the render thread processes the update.  This should use immediate commands instead.
	uint lastObject = s_objectBuffer.data.getMaxUsedId();
	if (!s_objectBuffer.hResource)
	{
		s_objectBuffer.hResource = RdrResourceSystem::CreateStructuredBuffer(
			s_objectBuffer.data.data(), 
			s_objectBuffer.data.kMaxEntries, 
			sizeof(VsPerObject), 
			RdrResourceAccessFlags::CpuRW_GpuRO,
			CREATE_NULL_BACKPOINTER);
	}
	else if (lastObject > 0)
	{
		RdrResourceCommandList& rResCommandList = rRenderer.GetResourceCommandList();
		rResCommandList.UpdateBuffer(s_objectBuffer.hResource, s_objectBuffer.data.data(), lastObject, CREATE_NULL_BACKPOINTER);
	}
}

void RdrInstancedObjectDataBuffer::FlipState()
{
	s_objectBuffer.currentFrame = !s_objectBuffer.currentFrame;
}

RdrResourceHandle RdrInstancedObjectDataBuffer::GetResourceHandle()
{
	return s_objectBuffer.hResource;
}
