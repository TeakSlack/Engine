#include "vk_buffer.h"
#include "logger.h"

AllocatedBuffer vk_create_buffer(
    VmaAllocator             allocator,
    VkDeviceSize             size,
    VkBufferUsageFlags       usage_flags,
    VmaMemoryUsage           memory_usage,
    VmaAllocationCreateFlags alloc_flags)
{
    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = size;
    buf_info.usage = usage_flags;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.usage = memory_usage;
    alloc_create_info.flags = alloc_flags;

    AllocatedBuffer result;
    VK_CHECK(vmaCreateBuffer(allocator, &buf_info, &alloc_create_info,
        &result.buffer, &result.allocation, &result.info));

    LOG_DEBUG_TO("render", "Buffer created: {} bytes, usage={:#x}",
        (u64)size, (u32)usage_flags);
    return result;
}

void vk_destroy_buffer(VmaAllocator allocator, AllocatedBuffer& buf) {
    vmaDestroyBuffer(allocator, buf.buffer, buf.allocation);
    buf.buffer = VK_NULL_HANDLE;
    buf.allocation = VK_NULL_HANDLE;
}

void vk_copy_buffer(VkContext ctx, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = ctx.transfer_cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &alloc_info, &cmd));

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    VkBufferCopy copy_region = {};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &copy_region);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    VK_CHECK(vkCreateFence(ctx.device, &fence_info, nullptr, &fence));

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(ctx.transfer_queue, 1, &submit_info, fence));

    VK_CHECK(vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX));
    vkFreeCommandBuffers(ctx.device, ctx.transfer_cmd_pool, 1, &cmd);
	vkDestroyFence(ctx.device, fence, nullptr);
}

AllocatedBuffer vk_create_staging_buffer(
    VmaAllocator allocator, const void* data, VkDeviceSize size)
{
    // HOST_ACCESS_SEQUENTIAL_WRITE tells VMA this is a CPU->GPU upload path.
    // VMA will pick a HOST_VISIBLE | HOST_COHERENT heap automatically.
    // MAPPED_BIT means VMA maps it for us and stores the pointer in info.pMappedData.
    AllocatedBuffer staging = vk_create_buffer(
        allocator, size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    memcpy(staging.info.pMappedData, data, (size_t)size);
    // No explicit flush needed — HOST_COHERENT memory is always visible to GPU
    return staging;
}

AllocatedImage vk_create_image(
    VmaAllocator       allocator,
    VkDevice           device,
    VkFormat           format,
    VkExtent2D         extent,
    VkImageUsageFlags  usage,
    VkImageAspectFlags aspect)
{
    VkImageCreateInfo img_info = {};
    img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_info.imageType = VK_IMAGE_TYPE_2D;
    img_info.format = format;
    img_info.extent = { extent.width, extent.height, 1 };
    img_info.mipLevels = 1;
    img_info.arrayLayers = 1;
    img_info.samples = VK_SAMPLE_COUNT_1_BIT;
    img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_info.usage = usage;
    img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // DEVICE_LOCAL is always correct for GPU-resident render targets/textures
    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    AllocatedImage result;
    result.format = format;
    result.extent = extent;
    VK_CHECK(vmaCreateImage(allocator, &img_info, &alloc_create_info,
        &result.image, &result.allocation, &result.info));

    // Create the image view immediately — it's almost always needed alongside
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = result.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &result.view));

    LOG_DEBUG_TO("render", "Image created: {}x{} fmt={} usage={:#x}",
        extent.width, extent.height, (int)format, (u32)usage);
    return result;
}

void vk_destroy_image(VmaAllocator allocator, VkDevice device, AllocatedImage& img) {
    vkDestroyImageView(device, img.view, nullptr);
    vmaDestroyImage(allocator, img.image, img.allocation);
    img.image = VK_NULL_HANDLE;
    img.view = VK_NULL_HANDLE;
    img.allocation = VK_NULL_HANDLE;
}