#include "TransientResourceSystem.h"
#include "Render/IGpuDevice.h"
#include "Render/FrameGraph/RenderResource.h"

void TransientResourceSystem::Acquire(IGpuDevice* device, RGResourceNode* resource)
{
	CORE_ASSERT(!resource->Culled, "Attempting to acquire a culled resource!");

	if (resource->Kind == RGResourceNode::ResourceKind::Texture)
	{
		CORE_ASSERT(!resource->ResolvedTexture.IsValid(), "Attempting to acquire an invalid resource!");
		resource->ResolvedTexture = device->CreateTexture(resource->TextureDesc);
	}
	else
	{
		CORE_ASSERT(!resource->ResolvedBuffer.IsValid(), "Attempting to acquire an invalid resource!");
		resource->ResolvedBuffer = device->CreateBuffer(resource->BufferDesc);
	}
}

void TransientResourceSystem::Release(IGpuDevice* device, RGResourceNode* resource)
{
	if (resource->Kind == RGResourceNode::ResourceKind::Texture)
	{
		if (!resource->ResolvedTexture.IsValid()) return;
		device->DestroyTexture(resource->ResolvedTexture);
		resource->ResolvedTexture = {};
	}
	else
	{
		if (!resource->ResolvedBuffer.IsValid()) return;
		device->DestroyBuffer(resource->ResolvedBuffer);
		resource->ResolvedBuffer = {};
	}
}

