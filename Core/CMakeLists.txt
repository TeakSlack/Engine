file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS "src/*.h")

add_library(Core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

# ---------------------------------------------------------------------------
# Includes
# PUBLIC: Core/src is propagated to any target that links Core, so App and
# DeferredRender can include engine headers without repeating the path.
# ---------------------------------------------------------------------------
target_include_directories(Core PUBLIC
    ${CMAKE_SOURCE_DIR}/Core/src
)

# Mirror filesystem structure in VS filters
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${CORE_SOURCES} ${CORE_HEADERS})

target_link_libraries(Core PUBLIC
    VendorIncludes
    Vulkan::Vulkan
    glfw
)

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
target_compile_definitions(Core PRIVATE
    $<$<CONFIG:Debug>:DEBUG;_DEBUG>
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
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_SOURCE_DIR}/lib"
)
