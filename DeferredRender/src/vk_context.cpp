#include "vk_context.h"
#include <GLFW/glfw3.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <algorithm>

// -------------------------------------------------------------------------
// Device extensions we require on the chosen physical device.
// -------------------------------------------------------------------------
static constexpr const char* DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
static constexpr u32 DEVICE_EXTENSION_COUNT =
    (u32)(sizeof(DEVICE_EXTENSIONS) / sizeof(DEVICE_EXTENSIONS[0]));

// -------------------------------------------------------------------------
// Forward declarations of internal helpers
// -------------------------------------------------------------------------
static void               create_instance(VkContext& ctx);
static void               setup_debug_messenger(VkContext& ctx);
static void               create_surface(VkContext& ctx, GLFWwindow* window);
static void               pick_physical_device(VkContext& ctx);
static void               create_logical_device(VkContext& ctx);
static bool               is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface);
static bool               check_device_extension_support(VkPhysicalDevice device);
static int                score_device(VkPhysicalDevice device);

// =========================================================================
// Public API
// =========================================================================

void vk_context_init(VkContext& ctx, void* glfw_window)
{
    create_instance(ctx);

#ifdef VK_ENABLE_VALIDATION
    setup_debug_messenger(ctx);
#endif

    create_surface(ctx, (GLFWwindow*)glfw_window);
    pick_physical_device(ctx);
    create_logical_device(ctx);

    // Cache device properties and memory layout for later use
    vkGetPhysicalDeviceProperties(ctx.physical_device, &ctx.device_props);
    vkGetPhysicalDeviceMemoryProperties(ctx.physical_device, &ctx.memory_props);

    printf("[VkContext] Using GPU: %s\n", ctx.device_props.deviceName);
}

void vk_context_destroy(VkContext& ctx)
{
    vkDestroyDevice(ctx.device, nullptr);

#ifdef VK_ENABLE_VALIDATION
    vk_destroy_debug_messenger(ctx.instance, ctx.debug_messenger, nullptr);
#endif

    vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
    vkDestroyInstance(ctx.instance, nullptr);
}

// =========================================================================
// QueueFamilyIndices
// =========================================================================

QueueFamilyIndices vk_find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;

    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (u32 i = 0; i < count; ++i) {
        // Graphics
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics = i;

        // Present support
        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support)
            indices.present = i;

        // Prefer a dedicated transfer-only queue
        bool is_transfer = (families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;
        bool not_graphics = (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0;
        if (is_transfer && not_graphics)
            indices.transfer = i;
    }

    // Fall back to graphics queue for transfers if no dedicated one
    if (indices.transfer == UINT32_MAX)
        indices.transfer = indices.graphics;

    return indices;
}

// =========================================================================
// Memory helper
// =========================================================================

u32 vk_find_memory_type(const VkContext& ctx, u32 type_filter,
                          VkMemoryPropertyFlags properties)
{
    for (u32 i = 0; i < ctx.memory_props.memoryTypeCount; ++i) {
        bool type_match = (type_filter & (1u << i)) != 0;
        bool prop_match = (ctx.memory_props.memoryTypes[i].propertyFlags & properties) == properties;
        if (type_match && prop_match)
            return i;
    }
    fprintf(stderr, "[VkContext] Failed to find suitable memory type\n");
    abort();
}

// =========================================================================
// Internal helpers
// =========================================================================

static void create_instance(VkContext& ctx)
{
#ifdef VK_ENABLE_VALIDATION
    if (!vk_check_validation_layer_support()) {
        fprintf(stderr, "[VkContext] Validation layers requested but not available\n");
        abort();
    }
#endif

    VkApplicationInfo app_info  = {};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "vk_renderer";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName        = "core_engine";
    app_info.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion         = VK_API_VERSION_1_3;  // require 1.3 for sync2 etc.

    // GLFW tells us which instance extensions it needs for surface creation
    u32          glfw_ext_count = 0;
    const char** glfw_exts      = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    std::vector<const char*> extensions(glfw_exts, glfw_exts + glfw_ext_count);

#ifdef VK_ENABLE_VALIDATION
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo create_info = {};
    create_info.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo     = &app_info;
    create_info.enabledExtensionCount   = (u32)extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();

    // Chain the debug messenger into instance creation so we catch any
    // errors that occur during vkCreateInstance / vkDestroyInstance itself.
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};

#ifdef VK_ENABLE_VALIDATION
    vk_populate_debug_messenger_create_info(debug_create_info);
    create_info.enabledLayerCount   = VALIDATION_LAYER_COUNT;
    create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    create_info.pNext               = &debug_create_info;
#endif

    VK_CHECK(vkCreateInstance(&create_info, nullptr, &ctx.instance));
}

static void setup_debug_messenger(VkContext& ctx)
{
    VkDebugUtilsMessengerCreateInfoEXT info = {};
    vk_populate_debug_messenger_create_info(info);
    VK_CHECK(vk_create_debug_messenger(ctx.instance, &info, nullptr, &ctx.debug_messenger));
}

static void create_surface(VkContext& ctx, GLFWwindow* window)
{
    // glfwCreateWindowSurface handles the platform-specific call
    // (Win32, Xlib, Wayland, Metal — depending on build target)
    VK_CHECK(glfwCreateWindowSurface(ctx.instance, window, nullptr, &ctx.surface));
}

static bool check_device_extension_support(VkPhysicalDevice device)
{
    u32 count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    for (const char* required : DEVICE_EXTENSIONS) {
        bool found = false;
        for (const auto& ext : available) {
            if (strcmp(required, ext.extensionName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

static bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices = vk_find_queue_families(device, surface);
    if (!indices.is_complete()) return false;
    if (!check_device_extension_support(device)) return false;

    // Ensure swapchain has at least one supported format and present mode
    u32 format_count = 0, mode_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &mode_count, nullptr);
    if (format_count == 0 || mode_count == 0) return false;

    return true;
}

static int score_device(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    int score = 0;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   score += 10000;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 1000;
    score += (int)(props.limits.maxImageDimension2D / 1024);
    return score;
}

static void pick_physical_device(VkContext& ctx)
{
    u32 count = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &count, nullptr);
    if (count == 0) {
        fprintf(stderr, "[VkContext] No Vulkan-capable GPUs found\n");
        abort();
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(ctx.instance, &count, devices.data());

    // Filter unsuitable devices, then pick the highest-scoring one
    int   best_score  = -1;
    for (VkPhysicalDevice d : devices) {
        if (!is_device_suitable(d, ctx.surface)) continue;
        int s = score_device(d);
        if (s > best_score) {
            best_score         = s;
            ctx.physical_device = d;
        }
    }

    if (ctx.physical_device == VK_NULL_HANDLE) {
        fprintf(stderr, "[VkContext] No suitable GPU found\n");
        abort();
    }
}

static void create_logical_device(VkContext& ctx)
{
    ctx.queue_families = vk_find_queue_families(ctx.physical_device, ctx.surface);

    // Collect unique queue family indices — some may share the same index
    std::vector<u32> unique_families;
    auto add_unique = [&](u32 idx) {
        if (std::find(unique_families.begin(), unique_families.end(), idx) == unique_families.end())
            unique_families.push_back(idx);
    };
    add_unique(ctx.queue_families.graphics);
    add_unique(ctx.queue_families.present);
    add_unique(ctx.queue_families.transfer);

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    for (u32 family : unique_families) {
        VkDeviceQueueCreateInfo qi = {};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queue_infos.push_back(qi);
    }

    // Features — expand as you need them in later phases
    VkPhysicalDeviceFeatures features = {};
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid  = VK_TRUE; // wireframe debug rendering

    VkDeviceCreateInfo create_info      = {};
    create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount    = (u32)queue_infos.size();
    create_info.pQueueCreateInfos       = queue_infos.data();
    create_info.enabledExtensionCount   = DEVICE_EXTENSION_COUNT;
    create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS;
    create_info.pEnabledFeatures        = &features;

#ifdef VK_ENABLE_VALIDATION
    // Older drivers require validation layers repeated on the device
    create_info.enabledLayerCount   = VALIDATION_LAYER_COUNT;
    create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
#endif

    VK_CHECK(vkCreateDevice(ctx.physical_device, &create_info, nullptr, &ctx.device));

    vkGetDeviceQueue(ctx.device, ctx.queue_families.graphics, 0, &ctx.graphics_queue);
    vkGetDeviceQueue(ctx.device, ctx.queue_families.present,  0, &ctx.present_queue);
    vkGetDeviceQueue(ctx.device, ctx.queue_families.transfer, 0, &ctx.transfer_queue);
}
