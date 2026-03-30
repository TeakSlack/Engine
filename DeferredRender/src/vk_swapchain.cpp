#include "vk_swapchain.h"
#include "logger.h"
#include <vector>
#include <algorithm>

// -------------------------------------------------------------------------
// Internal helpers
// -------------------------------------------------------------------------

static VkSurfaceFormatKHR choose_surface_format(
    const std::vector<VkSurfaceFormatKHR>& formats)
{
    // Prefer sRGB + B8G8R8A8 - this is the most common and gives correct
    // gamma-corrected display output without manual gamma correction.
    for (const auto& f : formats) {
        if (f.format     == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    // Fallback: whatever the driver offers first
    return formats[0];
}

static VkPresentModeKHR choose_present_mode(
    const std::vector<VkPresentModeKHR>& modes)
{
    // Mailbox = triple buffering, low latency, no tearing
    for (auto m : modes)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    // FIFO is always guaranteed to be supported - use as fallback (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_extent(
    const VkSurfaceCapabilitiesKHR& caps, u32 width, u32 height)
{
    // If currentExtent is UINT32_MAX the surface lets us choose freely
    if (caps.currentExtent.width != UINT32_MAX)
        return caps.currentExtent;

    VkExtent2D extent = { width, height };
    extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

static void create_image_views(VkSwapchain& sc, VkDevice device)
{
    sc.image_views.resize(sc.images.size());
    for (u32 i = 0; i < (u32)sc.images.size(); ++i) {
        VkImageViewCreateInfo info = {};
        info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image                           = sc.images[i];
        info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        info.format                          = sc.format;
        info.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel   = 0;
        info.subresourceRange.levelCount     = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount     = 1;
        VK_CHECK(vkCreateImageView(device, &info, nullptr, &sc.image_views[i]));
    }
}

static void create_sync_objects(VkSwapchain& sc, VkDevice device)
{
    VkSemaphoreCreateInfo sem_info = {};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // Fences start signaled so the first vkWaitForFences doesn't block
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(device, &sem_info, nullptr, &sc.image_available_semaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &sem_info, nullptr, &sc.render_finished_semaphores[i]));
        VK_CHECK(vkCreateFence   (device, &fence_info, nullptr, &sc.in_flight_fences[i]));
    }
}

// =========================================================================
// Public API
// =========================================================================

void vk_swapchain_create(VkSwapchain& sc, const VkContext& ctx, u32 width, u32 height)
{
    // Query surface capabilities, formats, and present modes
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physical_device, ctx.surface, &capabilities);

    u32 format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &format_count, formats.data());

    u32 mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &mode_count, nullptr);
    std::vector<VkPresentModeKHR> modes(mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &mode_count, modes.data());

    VkSurfaceFormatKHR surface_format = choose_surface_format(formats);
    VkPresentModeKHR   present_mode   = choose_present_mode(modes);
    VkExtent2D         extent         = choose_extent(capabilities, width, height);

    // Request one more image than the minimum to avoid stalling on the driver
    u32 image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0)
        image_count = 0;
        //image_count = std::min(image_count, capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface          = ctx.surface;
    create_info.minImageCount    = image_count;
    create_info.imageFormat      = surface_format.format;
    create_info.imageColorSpace  = surface_format.colorSpace;
    create_info.imageExtent      = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // If graphics and present are on different queue families, images must
    // be shared (concurrent) rather than exclusively owned.
    u32 qf_indices[] = { ctx.queue_families.graphics, ctx.queue_families.present };
    if (ctx.queue_families.graphics != ctx.queue_families.present) {
        create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices   = qf_indices;
    } else {
        create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform   = capabilities.currentTransform; // no extra rotation
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode    = present_mode;
    create_info.clipped        = VK_TRUE; // don't care about pixels behind other windows

    VK_CHECK(vkCreateSwapchainKHR(ctx.device, &create_info, nullptr, &sc.handle));

    // Retrieve swapchain images (count may exceed our minImageCount request)
    u32 actual_count = 0;
    vkGetSwapchainImagesKHR(ctx.device, sc.handle, &actual_count, nullptr);
    sc.images.resize(actual_count);
    vkGetSwapchainImagesKHR(ctx.device, sc.handle, &actual_count, sc.images.data());

    sc.format = surface_format.format;
    sc.extent = extent;

    create_image_views(sc, ctx.device);
    create_sync_objects(sc, ctx.device);

    const char* mode_str =
        (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)  ? "mailbox (triple-buffer)" :
        (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) ? "immediate (no vsync)"    :
                                                          "fifo (vsync)";

    LOG_INFO_TO("vulkan", "Swapchain ready with {} images, {}x{}", actual_count,
                extent.width, extent.height);
    LOG_DEBUG_TO("vulkan", "  Present mode : {}", mode_str);
    LOG_DEBUG_TO("vulkan", "  Image format : {}", (int)surface_format.format);
    LOG_DEBUG_TO("vulkan", "  Color space  : {}", (int)surface_format.colorSpace);
    LOG_DEBUG_TO("vulkan", "  Sharing mode : {}",
                 create_info.imageSharingMode == VK_SHARING_MODE_CONCURRENT
                 ? "concurrent (separate present queue)"
                 : "exclusive (shared queue)");
}

void vk_swapchain_destroy(VkSwapchain& sc, const VkContext& ctx)
{
    LOG_DEBUG_TO("vulkan", "Destroying swapchain");
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(ctx.device, sc.image_available_semaphores[i], nullptr);
        vkDestroySemaphore(ctx.device, sc.render_finished_semaphores[i], nullptr);
        vkDestroyFence    (ctx.device, sc.in_flight_fences[i],           nullptr);
    }
    for (auto view : sc.image_views)
        vkDestroyImageView(ctx.device, view, nullptr);
    vkDestroySwapchainKHR(ctx.device, sc.handle, nullptr);

    sc.images.clear();
    sc.image_views.clear();
    sc.handle = VK_NULL_HANDLE;
}

void vk_swapchain_recreate(VkSwapchain& sc, const VkContext& ctx, u32 width, u32 height)
{
    LOG_INFO_TO("vulkan", "Recreating swapchain at {}x{}", width, height);
    vkDeviceWaitIdle(ctx.device);
    vk_swapchain_destroy(sc, ctx);
    vk_swapchain_create(sc, ctx, width, height);
}

// =========================================================================
// Per-frame helpers
// =========================================================================

u32 vk_swapchain_acquire(VkSwapchain& sc, const VkContext& ctx)
{
    // Wait for the command buffer from this frame slot to finish executing
    vkWaitForFences(ctx.device, 1, &sc.in_flight_fences[sc.current_frame],
                    VK_TRUE, UINT64_MAX);

    u32      image_index = 0;
    VkResult result = vkAcquireNextImageKHR(
        ctx.device, sc.handle, UINT64_MAX,
        sc.image_available_semaphores[sc.current_frame],
        VK_NULL_HANDLE,
        &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        return UINT32_MAX; // caller should recreate

    VK_CHECK(result);

    // Reset fence only after we know we're submitting work this frame
    vkResetFences(ctx.device, 1, &sc.in_flight_fences[sc.current_frame]);

    return image_index;
}

bool vk_swapchain_submit_and_present(
    VkSwapchain&        sc,
    const VkContext&    ctx,
    VkCommandBuffer     cmd,
    u32                 image_index)
{
    // --------------- submit ---------------
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSubmitInfo submit = {};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &sc.image_available_semaphores[sc.current_frame];
    submit.pWaitDstStageMask    = wait_stages;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &sc.render_finished_semaphores[sc.current_frame];

    VK_CHECK(vkQueueSubmit(ctx.graphics_queue, 1, &submit,
                           sc.in_flight_fences[sc.current_frame]));

    // --------------- present ---------------
    VkPresentInfoKHR present = {};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &sc.render_finished_semaphores[sc.current_frame];
    present.swapchainCount     = 1;
    present.pSwapchains        = &sc.handle;
    present.pImageIndices      = &image_index;

    VkResult result = vkQueuePresentKHR(ctx.present_queue, &present);

    bool needs_recreate = (result == VK_ERROR_OUT_OF_DATE_KHR ||
                           result == VK_SUBOPTIMAL_KHR);
    if (!needs_recreate) VK_CHECK(result);

    // Advance to the next frame slot
    sc.current_frame = (sc.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

    return !needs_recreate;
}
