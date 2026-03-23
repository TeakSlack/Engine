#include "vk_debug.h"
#include <cstring>
#include <cstdio>
#include <vector>

// -------------------------------------------------------------------------
// Debug callback — receives messages from the validation layer and prints
// them. Severity levels map to: verbose (spam) / info / warning / error.
// -------------------------------------------------------------------------
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*                                       /*user_data*/)
{
    // Filter out verbose noise — only print info and above.
    if (severity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        return VK_FALSE;

    const char* prefix =
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   ? "[VK ERROR]"   :
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "[VK WARNING]" :
                                                                        "[VK INFO]";

    fprintf(stderr, "%s %s\n", prefix, data->pMessage);
    return VK_FALSE; // VK_TRUE would abort the call that triggered this — only useful for testing
}

// -------------------------------------------------------------------------
void vk_populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT& info)
{
    info = {};
    info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType     =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debug_callback;
}

// -------------------------------------------------------------------------
bool vk_check_validation_layer_support()
{
    u32 layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available.data());

    for (const char* name : VALIDATION_LAYERS) {
        bool found = false;
        for (const auto& props : available) {
            if (strcmp(name, props.layerName) == 0) { found = true; break; }
        }
        if (!found) {
            fprintf(stderr, "[vk_debug] Validation layer not found: %s\n", name);
            return false;
        }
    }
    return true;
}

// -------------------------------------------------------------------------
// vkCreateDebugUtilsMessengerEXT is an extension function — it must be
// looked up at runtime via vkGetInstanceProcAddr.
// -------------------------------------------------------------------------
VkResult vk_create_debug_messenger(
    VkInstance                                instance,
    const VkDebugUtilsMessengerCreateInfoEXT* create_info,
    const VkAllocationCallbacks*              allocator,
    VkDebugUtilsMessengerEXT*                 out_messenger)
{
    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (!fn) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return fn(instance, create_info, allocator, out_messenger);
}

void vk_destroy_debug_messenger(
    VkInstance                   instance,
    VkDebugUtilsMessengerEXT     messenger,
    const VkAllocationCallbacks* allocator)
{
    auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (fn) fn(instance, messenger, allocator);
}
