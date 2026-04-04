# Gather shared sources, stripping backend files so they can be added back
# only when their option is ON. Backend sources are collected separately so
# that add_library and source_group both see the final combined list —
# keeping Solution Explorer in sync with what actually compiles.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS "src/*.h")

list(FILTER CORE_SOURCES EXCLUDE REGEX "VulkanDevice\\.cpp$")
list(FILTER CORE_SOURCES EXCLUDE REGEX "D3D12Device\\.cpp$")

set(BACKEND_SOURCES)
if(CORE_VULKAN)
    list(APPEND BACKEND_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/VulkanDevice.cpp")
endif()
if(CORE_DX12)
    list(APPEND BACKEND_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/D3D12Device.cpp")
endif()

add_library(Core STATIC ${CORE_SOURCES} ${BACKEND_SOURCES} ${CORE_HEADERS})

# ---------------------------------------------------------------------------
# Includes
# PUBLIC: Core/src is propagated to any target that links Core, so App
# can include engine headers without repeating the path.
# ---------------------------------------------------------------------------
target_include_directories(Core PUBLIC
    ${CMAKE_SOURCE_DIR}/Core/src
)

# Mirror filesystem structure in VS filters — covers both shared and backend
# sources so Solution Explorer matches the enabled configuration exactly.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${CORE_SOURCES} ${BACKEND_SOURCES} ${CORE_HEADERS})

# Base libraries — always required regardless of backend
target_link_libraries(Core PUBLIC
    VendorIncludes
    glfw
    nvrhi
)

# ---------------------------------------------------------------------------
# Backend-specific libraries and macros
# PUBLIC compile definitions so App (and any other consumer of Core) sees
# the same CORE_VULKAN / CORE_DX12 macros without re-declaring them.
# ---------------------------------------------------------------------------
if(CORE_VULKAN)
    target_link_libraries(Core PUBLIC Vulkan::Vulkan nvrhi_vk)
    target_compile_definitions(Core PUBLIC CORE_VULKAN)
endif()

if(CORE_DX12)
    target_link_libraries(Core PUBLIC nvrhi_d3d12 dxgi)
    target_compile_definitions(Core PUBLIC CORE_DX12)
endif()

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
target_compile_definitions(Core PRIVATE
    $<$<CONFIG:Debug>:DEBUG;_DEBUG>
    $<$<CONFIG:Development>:DEVELOPMENT>
    $<$<CONFIG:Release>:NDEBUG>
)

if(WIN32)
    target_compile_definitions(Core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(Core PRIVATE /utf-8)
else()
    target_compile_options(Core PRIVATE -Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# Output — flat lib/ directory at workspace root, matching premake layout
# ---------------------------------------------------------------------------
set_property(TARGET Core PROPERTY FOLDER "Projects")

set_target_properties(Core PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY                "${CMAKE_SOURCE_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_SOURCE_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEVELOPMENT    "${CMAKE_SOURCE_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_SOURCE_DIR}/lib"
)
