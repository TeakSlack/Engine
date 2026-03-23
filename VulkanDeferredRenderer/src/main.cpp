#include "vk_context.h"
#include "vk_swapchain.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdio>
#include <vector>

static constexpr u32 INITIAL_WIDTH  = 1280;
static constexpr u32 INITIAL_HEIGHT = 720;

// -------------------------------------------------------------------------
// CommandPool + per-frame command buffers
// One pool per frame-in-flight keeps reset operations cheap —
// you reset the whole pool at once rather than individual buffers.
// -------------------------------------------------------------------------
struct FrameData {
    VkCommandPool   command_pool   = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
};

static FrameData frame_data[MAX_FRAMES_IN_FLIGHT] = {};

static void create_command_infrastructure(const VkContext& ctx)
{
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = ctx.queue_families.graphics;
        // RESET_COMMAND_BUFFER_BIT lets us re-record without resetting the whole pool.
        // We're resetting the pool anyway, but this flag gives flexibility.
        pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(ctx.device, &pool_info, nullptr,
                                     &frame_data[i].command_pool));

        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = frame_data[i].command_pool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(ctx.device, &alloc_info,
                                          &frame_data[i].command_buffer));
    }
}

static void destroy_command_infrastructure(const VkContext& ctx)
{
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyCommandPool(ctx.device, frame_data[i].command_pool, nullptr);
    }
}

// -------------------------------------------------------------------------
// Record the command buffer for this frame.
// Phase 1: just clear the swapchain image to a dark color.
// In Phase 2 you'll add a render pass + draw calls here.
// -------------------------------------------------------------------------
static void record_commands(VkCommandBuffer cmd, VkImage swapchain_image)
{
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // ONE_TIME_SUBMIT: we re-record every frame — fine for now
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

    // -------------------------------------------------------------------
    // Transition swapchain image: UNDEFINED → TRANSFER_DST
    // We need to clear it, and vkCmdClearColorImage requires this layout.
    // In Phase 2 this will be replaced by a render pass load-op clear.
    // -------------------------------------------------------------------
    VkImageMemoryBarrier barrier_to_clear = {};
    barrier_to_clear.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_to_clear.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier_to_clear.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_clear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_to_clear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_to_clear.image               = swapchain_image;
    barrier_to_clear.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier_to_clear.srcAccessMask       = 0;
    barrier_to_clear.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr,
        1, &barrier_to_clear);

    // Clear to a dark teal — easily distinguishable from a black crash
    VkClearColorValue clear_color = {};
    clear_color.float32[0] = 0.05f; // R
    clear_color.float32[1] = 0.10f; // G
    clear_color.float32[2] = 0.12f; // B
    clear_color.float32[3] = 1.00f; // A

    VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdClearColorImage(cmd, swapchain_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &clear_color, 1, &range);

    // -------------------------------------------------------------------
    // Transition swapchain image: TRANSFER_DST → PRESENT_SRC
    // Required before vkQueuePresentKHR.
    // -------------------------------------------------------------------
    VkImageMemoryBarrier barrier_to_present = barrier_to_clear;
    barrier_to_present.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_present.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier_to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier_to_present.dstAccessMask = 0;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr,
        1, &barrier_to_present);

    VK_CHECK(vkEndCommandBuffer(cmd));
}

// -------------------------------------------------------------------------
// Window resize callback
// -------------------------------------------------------------------------
static bool g_framebuffer_resized = false;
static void framebuffer_resize_callback(GLFWwindow*, int, int)
{
    g_framebuffer_resized = true;
}

// =========================================================================
// main
// =========================================================================
int main()
{
    // --------------- window ---------------
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan — no OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(INITIAL_WIDTH, INITIAL_HEIGHT,
                                          "vk_renderer | phase 1", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);

    // --------------- Vulkan init ---------------
    VkContext   ctx = {};
    VkSwapchain sc  = {};

    vk_context_init(ctx, window);
    vk_swapchain_create(sc, ctx, INITIAL_WIDTH, INITIAL_HEIGHT);
    create_command_infrastructure(ctx);

    // --------------- main loop ---------------
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Handle minimization — vkAcquireNextImageKHR fails on a 0x0 surface
        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        if (fb_w == 0 || fb_h == 0) continue;

        // Acquire next swapchain image
        u32 image_index = vk_swapchain_acquire(sc, ctx);
        if (image_index == UINT32_MAX || g_framebuffer_resized) {
            g_framebuffer_resized = false;
            vk_swapchain_recreate(sc, ctx, (u32)fb_w, (u32)fb_h);
            continue;
        }

        // Record and submit
        VkCommandBuffer cmd = frame_data[sc.current_frame].command_buffer;
        vkResetCommandBuffer(cmd, 0);
        record_commands(cmd, sc.images[image_index]);

        bool ok = vk_swapchain_submit_and_present(sc, ctx, cmd, image_index);
        if (!ok) {
            glfwGetFramebufferSize(window, &fb_w, &fb_h);
            vk_swapchain_recreate(sc, ctx, (u32)fb_w, (u32)fb_h);
        }
    }

    // --------------- cleanup ---------------
    // Wait for all GPU work to finish before destroying anything
    vkDeviceWaitIdle(ctx.device);

    destroy_command_infrastructure(ctx);
    vk_swapchain_destroy(sc, ctx);
    vk_context_destroy(ctx);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
