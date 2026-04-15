cmake_minimum_required(VERSION 3.20)
project(DeferredRender CXX C)

# ---------------------------------------------------------------------------
# Development build configuration (Gregory "Game Engine Architecture" §3)
# Sits between Debug and Release: fully optimised so the engine runs at real
# speed, but asserts and dev-only tools remain active (no NDEBUG).
# Must be declared before any target is created so VS picks it up.
# ---------------------------------------------------------------------------
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_isMultiConfig)
    if(NOT "Development" IN_LIST CMAKE_CONFIGURATION_TYPES)
        list(APPEND CMAKE_CONFIGURATION_TYPES Development)
        set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}"
            CACHE STRING "Available build configurations" FORCE)
    endif()
endif()

if(MSVC)
    # Optimised + full debug info; deliberately omits /RTC1 (runtime checks
    # are incompatible with optimisation) and NDEBUG (keeps assert() alive).
    set(CMAKE_C_FLAGS_DEVELOPMENT   "/O2 /Ob2 /Zi" CACHE STRING "C flags (Development)"   FORCE)
    set(CMAKE_CXX_FLAGS_DEVELOPMENT "/O2 /Ob2 /Zi" CACHE STRING "C++ flags (Development)" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT    "/DEBUG /INCREMENTAL:NO" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT "/DEBUG /INCREMENTAL:NO" CACHE STRING "" FORCE)
    set(CMAKE_STATIC_LINKER_FLAGS_DEVELOPMENT ""                       CACHE STRING "" FORCE)
else()
    set(CMAKE_C_FLAGS_DEVELOPMENT   "-O2 -g" CACHE STRING "C flags (Development)"   FORCE)
    set(CMAKE_CXX_FLAGS_DEVELOPMENT "-O2 -g" CACHE STRING "C++ flags (Development)" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT    "" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT "" CACHE STRING "" FORCE)
    set(CMAKE_STATIC_LINKER_FLAGS_DEVELOPMENT "" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Solution folders — groups projects in the VS Solution Explorer
# ---------------------------------------------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---------------------------------------------------------------------------
# Language standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Backend selection
# These options drive NVRHI's own backend flags, control which device
# .cpp files are compiled in Core, and define the COMPILE_WITH_VULKAN /
# COMPILE_WITH_DX12 macros used by engine and application code.
# Must be declared before add_subdirectory(vendor/NVRHI).
# ---------------------------------------------------------------------------
option(COMPILE_WITH_VULKAN "Enable the Vulkan rendering backend"      ON)
option(COMPILE_WITH_DX12   "Enable the Direct3D 12 rendering backend" OFF)
option(COMPILE_WITH_ASSERT  "Enable assertions"                        ON)

if(NOT COMPILE_WITH_VULKAN AND NOT COMPILE_WITH_DX12)
    message(FATAL_ERROR "At least one rendering backend must be enabled (COMPILE_WITH_VULKAN or COMPILE_WITH_DX12).")
endif()

# ---------------------------------------------------------------------------
# Vulkan SDK — only required when the Vulkan backend is on
# Respects the VULKAN_SDK environment variable set by the setup scripts.
# ---------------------------------------------------------------------------
if(COMPILE_WITH_VULKAN)
    find_package(Vulkan REQUIRED)
    message(STATUS "Vulkan ${Vulkan_VERSION} found at ${Vulkan_INCLUDE_DIRS}")
endif()

# ---------------------------------------------------------------------------
# GLFW — built from source via its own CMakeLists.txt
# ---------------------------------------------------------------------------
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glfw)

# ---------------------------------------------------------------------------
# nvrhi — enable only the backends that are selected above
# ---------------------------------------------------------------------------
set(NVRHI_WITH_VULKAN ${COMPILE_WITH_VULKAN} CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX12   ${COMPILE_WITH_DX12}   CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX11   OFF                    CACHE BOOL "" FORCE)
set(NVRHI_INSTALL     OFF            CACHE BOOL "" FORCE)
add_subdirectory(vendor/NVRHI)

# ---------------------------------------------------------------------------
# fastgltf
# ---------------------------------------------------------------------------
set(FASTGLTF_COMPILE_AS_CPP20   ON  CACHE BOOL "" FORCE)
add_subdirectory(vendor/fastgltf)

# ---------------------------------------------------------------------------
# nlohmann json
# ---------------------------------------------------------------------------
add_subdirectory(vendor/json)

# ---------------------------------------------------------------------------
# Vendor include interface target
# Linking against this propagates all third-party include paths.
# Vulkan SDK headers are only added when the Vulkan backend is on.
# ---------------------------------------------------------------------------
add_library(VendorIncludes INTERFACE)
target_include_directories(VendorIncludes INTERFACE
    $<$<BOOL:${COMPILE_WITH_VULKAN}>:${Vulkan_INCLUDE_DIRS}>
    ${CMAKE_SOURCE_DIR}/vendor/spdlog/include
    ${CMAKE_SOURCE_DIR}/vendor/NVRHI/include
    ${CMAKE_SOURCE_DIR}/vendor/entt/src
    ${CMAKE_SOURCE_DIR}/vendor/fastgltf/include
    ${CMAKE_SOURCE_DIR}/vendor/stb
)

# ---------------------------------------------------------------------------
# Vendor solution folders
# ---------------------------------------------------------------------------
set_property(TARGET glfw PROPERTY FOLDER "Vendor")
if(TARGET update_mappings)
    set_property(TARGET update_mappings PROPERTY FOLDER "Vendor")
endif()
foreach(_nvrhi_target nvrhi nvrhi_vk nvrhi_d3d12 nvrhi_validation)
    if(TARGET ${_nvrhi_target})
        set_property(TARGET ${_nvrhi_target} PROPERTY FOLDER "Vendor/NVRHI")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Sub-projects
# ---------------------------------------------------------------------------
add_subdirectory(Core)
add_subdirectory(App)
add_subdirectory(EngineDLL)
