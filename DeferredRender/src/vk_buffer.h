#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include "vk_types.h"
#include "vk_context.h"
#include <vk_mem_alloc.h>

// A VkBuffer + its VMA backing allocation, kept together.
// Never separate them — you need both to free correctly.
struct AllocatedBuffer {
    VkBuffer      buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo info = {};  // cached: size, offset, pMappedData, etc.
};

// A VkImage + its VMA backing allocation.
struct AllocatedImage {
    VkImage       image = VK_NULL_HANDLE;
    VkImageView   view = VK_NULL_HANDLE;  // owned alongside for convenience
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo info = {};
    VkFormat      format = VK_FORMAT_UNDEFINED;
    VkExtent2D    extent = {};
};

// -------------------------------------------------------------------------
// Buffer helpers
// -------------------------------------------------------------------------

// General-purpose buffer creation.
// usage_flags:    e.g. VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
// memory_usage:   VMA_MEMORY_USAGE_AUTO is almost always correct —
//                 VMA picks DEVICE_LOCAL or HOST_VISIBLE based on usage hints.
// alloc_flags:    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT for staging,
//                 VMA_ALLOCATION_CREATE_MAPPED_BIT for persistently mapped UBOs.
AllocatedBuffer vk_create_buffer(
    VmaAllocator          allocator,
    VkDeviceSize          size,
    VkBufferUsageFlags    usage_flags,
    VmaMemoryUsage        memory_usage = VMA_MEMORY_USAGE_AUTO,
    VmaAllocationCreateFlags alloc_flags = 0);

void vk_destroy_buffer(VmaAllocator allocator, AllocatedBuffer& buf);

// Convenience: create a host-visible staging buffer, copy data into it.
AllocatedBuffer vk_create_staging_buffer(VmaAllocator allocator,
    const void* data, VkDeviceSize size);

void vk_copy_buffer(VkContext ctx, VkBuffer src, VkBuffer dst, VkDeviceSize size);

// -------------------------------------------------------------------------
// Image helpers (used from Phase 4 onward, declared here for completeness)
// -------------------------------------------------------------------------
AllocatedImage vk_create_image(
    VmaAllocator        allocator,
    VkDevice            device,
    VkFormat            format,
    VkExtent2D          extent,
    VkImageUsageFlags   usage,
    VkImageAspectFlags  aspect);

void vk_destroy_image(VmaAllocator allocator, VkDevice device, AllocatedImage& img);

#endif // VK_BUFFER_H