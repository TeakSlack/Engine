#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include "vk_types.h"
#include "vk_debug.h"
#include <vk_mem_alloc.h>

// -------------------------------------------------------------------------
// QueueFamilyIndices
// Stores the queue family index for each capability we need.
// A single family index can cover multiple roles on some hardware.
// -------------------------------------------------------------------------
struct QueueFamilyIndices {
    u32  graphics = UINT32_MAX;
    u32  present  = UINT32_MAX;
    u32  transfer = UINT32_MAX; // dedicated transfer, falls back to graphics

    bool is_complete() const {
        return graphics != UINT32_MAX && present != UINT32_MAX;
    }
};

// -------------------------------------------------------------------------
// VkContext
// Owns the Vulkan instance, physical device, logical device, queues, and
// surface. Lives for the entire application lifetime.
// -------------------------------------------------------------------------
struct VkContext {
    VkInstance               instance        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR             surface         = VK_NULL_HANDLE;

    VkPhysicalDevice         physical_device = VK_NULL_HANDLE;
    VkDevice                 device          = VK_NULL_HANDLE;

    QueueFamilyIndices       queue_families;
    VkQueue                  graphics_queue  = VK_NULL_HANDLE;
    VkQueue                  present_queue   = VK_NULL_HANDLE;
    VkQueue                  transfer_queue  = VK_NULL_HANDLE;

	VkCommandPool            transfer_cmd_pool = VK_NULL_HANDLE; // for one-time transfer commands
    VkPhysicalDeviceProperties       device_props      = {};
    VkPhysicalDeviceMemoryProperties memory_props      = {};

	VmaAllocator             allocator = VK_NULL_HANDLE;
};

// -------------------------------------------------------------------------
// Lifecycle
// Pass the GLFW window handle so the surface can be created from it.
// -------------------------------------------------------------------------
void vk_context_init(VkContext& ctx, void* glfw_window);
void vk_context_destroy(VkContext& ctx);

// -------------------------------------------------------------------------
// Helpers used by vk_swapchain and later layers
// -------------------------------------------------------------------------
QueueFamilyIndices vk_find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);

//u32 vk_find_memory_type(const VkContext& ctx, u32 type_filter,
//                         VkMemoryPropertyFlags properties);

#endif // VK_CONTEXT_H