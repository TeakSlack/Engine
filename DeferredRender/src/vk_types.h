#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cassert>

// -------------------------------------------------------------------------
// Primitive types
// -------------------------------------------------------------------------
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s32 = int32_t;
using f32 = float;
using b32 = s32;

// -------------------------------------------------------------------------
// VK_CHECK — wraps every Vulkan call and aborts on error.
// In a real engine this would route through your assert/logging system.
// -------------------------------------------------------------------------
#define VK_CHECK(call)                                                        \
    do {                                                                      \
        VkResult _vk_result = (call);                                         \
        if (_vk_result != VK_SUCCESS) {                                       \
            fprintf(stderr, "[VK_CHECK] %s failed with VkResult %d\n"        \
                            "  at %s:%d\n",                                   \
                    #call, (int)_vk_result, __FILE__, __LINE__);              \
            abort();                                                          \
        }                                                                     \
    } while (0)

// -------------------------------------------------------------------------
// Tuning constants
// -------------------------------------------------------------------------
static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;
