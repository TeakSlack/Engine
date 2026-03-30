cmake_minimum_required(VERSION 3.20)
project(DeferredRender CXX C)

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
# Vulkan SDK
# Respects the VULKAN_SDK environment variable set by the setup scripts.
# find_package picks up whatever version the setup scripts installed, which
# always tracks the latest release fetched from vulkan.lunarg.com.
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan ${Vulkan_VERSION} found at ${Vulkan_INCLUDE_DIRS}")

# ---------------------------------------------------------------------------
# GLFW — built from source via its own CMakeLists.txt
# ---------------------------------------------------------------------------
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glfw)

# ---------------------------------------------------------------------------
# nvrhi — Vulkan backend only; disable DX11/DX12 (submodules not needed)
# ---------------------------------------------------------------------------
set(NVRHI_WITH_VULKAN   ON  CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX11     OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX12     OFF CACHE BOOL "" FORCE)
set(NVRHI_INSTALL       OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/NVRHI)

# ---------------------------------------------------------------------------
# Vendor include interface target
# Linking against this propagates all third-party include paths.
# ---------------------------------------------------------------------------
add_library(VendorIncludes INTERFACE)
target_include_directories(VendorIncludes INTERFACE
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/vendor/spdlog/include
    ${CMAKE_SOURCE_DIR}/vendor/glm
    ${CMAKE_SOURCE_DIR}/vendor/stb
    ${CMAKE_SOURCE_DIR}/vendor/VulkanMemoryAllocator/include
    ${CMAKE_SOURCE_DIR}/vendor/tinyobjloader
    ${CMAKE_SOURCE_DIR}/vendor/NVRHI/include
)

# ---------------------------------------------------------------------------
# Vendor solution folders
# ---------------------------------------------------------------------------
set_property(TARGET glfw PROPERTY FOLDER "Vendor")
if(TARGET update_mappings)
    set_property(TARGET update_mappings PROPERTY FOLDER "Vendor")
endif()
foreach(_nvrhi_target nvrhi nvrhi_vk nvrhi_validation)
    if(TARGET ${_nvrhi_target})
        set_property(TARGET ${_nvrhi_target} PROPERTY FOLDER "Vendor/NVRHI")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Sub-projects
# ---------------------------------------------------------------------------
add_subdirectory(Core)
add_subdirectory(DeferredRender)
add_subdirectory(App)
