#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstdlib>

// logger.h is included here so every translation unit that includes
// vk_types.h automatically gets the log macros — no extra include needed.
#include "logger.h"

// -------------------------------------------------------------------------
// Primitive types
// -------------------------------------------------------------------------
using u8   = uint8_t;
using u16  = uint16_t;
using u32  = uint32_t;
using u64  = uint64_t;
using s32  = int32_t;
using f32  = float;
using b32  = s32;

// -------------------------------------------------------------------------
// VK_CHECK — wraps every Vulkan call.
// On failure: logs to the "vulkan" logger at Fatal level (with source
// location always visible regardless of verbose mode), then aborts.
// -------------------------------------------------------------------------
#define VK_CHECK(call)                                                          \
    do {                                                                        \
        VkResult _vk_result = (call);                                           \
        if (_vk_result != VK_SUCCESS) {                                         \
            LOG_FATAL_TO("vulkan",                                              \
                "VK_CHECK failed: {} returned VkResult {} at {}:{}",            \
                #call, (int)_vk_result, __FILE__, __LINE__);                    \
            abort();                                                            \
        }                                                                       \
    } while (0)

// -------------------------------------------------------------------------
// Tuning constants
// -------------------------------------------------------------------------
static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

#ifdef DEBUG
#define VK_ENABLE_VALIDATION
#endif

#endif // VK_TYPES_H