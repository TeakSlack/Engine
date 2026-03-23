#pragma once

#include "vk_types.h"

// -------------------------------------------------------------------------
// Validation layers to request in debug builds.
// -------------------------------------------------------------------------
static constexpr const char* VALIDATION_LAYERS[] = {
    "VK_LAYER_KHRONOS_validation",
};
static constexpr u32 VALIDATION_LAYER_COUNT =
    (u32)(sizeof(VALIDATION_LAYERS) / sizeof(VALIDATION_LAYERS[0]));

// -------------------------------------------------------------------------
// Check that all requested validation layers are available on this driver.
// -------------------------------------------------------------------------
bool vk_check_validation_layer_support();

// -------------------------------------------------------------------------
// Create / destroy the debug messenger extension object.
// Must be called after vkCreateInstance, before vkDestroyInstance.
// -------------------------------------------------------------------------
VkResult vk_create_debug_messenger(
    VkInstance                                instance,
    const VkDebugUtilsMessengerCreateInfoEXT* create_info,
    const VkAllocationCallbacks*              allocator,
    VkDebugUtilsMessengerEXT*                 out_messenger);

void vk_destroy_debug_messenger(
    VkInstance                   instance,
    VkDebugUtilsMessengerEXT     messenger,
    const VkAllocationCallbacks* allocator);

// -------------------------------------------------------------------------
// Fill in the create-info struct so it can also be chained into
// VkInstanceCreateInfo for catching instance-creation errors.
// -------------------------------------------------------------------------
void vk_populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT& info);
