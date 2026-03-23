#pragma once

#include "vk_types.h"
#include "vk_context.h"
#include <vector>

// -------------------------------------------------------------------------
// VkSwapchain
// Owns the VkSwapchainKHR, all image views, and the per-frame
// synchronization objects (semaphores + fences).
// -------------------------------------------------------------------------
struct VkSwapchain {
    VkSwapchainKHR           handle      = VK_NULL_HANDLE;
    VkFormat                 format      = VK_FORMAT_UNDEFINED;
    VkExtent2D               extent      = {};

    std::vector<VkImage>     images;      // owned by the swapchain
    std::vector<VkImageView> image_views; // we own these

    // Per-frame-in-flight sync objects
    // index by current_frame, NOT by swapchain image index
    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT] = {};
    VkFence     in_flight_fences[MAX_FRAMES_IN_FLIGHT]           = {};

    u32 current_frame = 0; // 0 .. MAX_FRAMES_IN_FLIGHT-1
};

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------
void vk_swapchain_create(VkSwapchain& sc, const VkContext& ctx, u32 width, u32 height);
void vk_swapchain_destroy(VkSwapchain& sc, const VkContext& ctx);

// Recreate after a resize or VK_ERROR_OUT_OF_DATE_KHR
void vk_swapchain_recreate(VkSwapchain& sc, const VkContext& ctx, u32 width, u32 height);

// -------------------------------------------------------------------------
// Per-frame helpers
// -------------------------------------------------------------------------

// Wait for the previous use of this frame slot to finish, then acquire
// the next swapchain image.  Returns the swapchain image index.
// Returns UINT32_MAX if the swapchain is out of date (caller should recreate).
u32  vk_swapchain_acquire(VkSwapchain& sc, const VkContext& ctx);

// Submit a command buffer and present.
// Returns false if the swapchain needs recreation.
bool vk_swapchain_submit_and_present(
    VkSwapchain&        sc,
    const VkContext&    ctx,
    VkCommandBuffer     cmd,
    u32                 image_index);
