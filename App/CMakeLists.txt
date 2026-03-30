file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE APP_HEADERS CONFIGURE_DEPENDS "src/*.h")

add_executable(App ${APP_SOURCES} ${APP_HEADERS})

# Mirror filesystem structure in VS filters
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${APP_SOURCES} ${APP_HEADERS})

# Core brings in Core/src and VendorIncludes transitively
target_link_libraries(App PRIVATE
    Core
    Vulkan::Vulkan
    glfw
)

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
target_compile_definitions(App PRIVATE
    $<$<CONFIG:Debug>:DEBUG;_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

if(WIN32)
    target_compile_definitions(App PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    target_compile_options(App PRIVATE /utf-8)
else()
    target_compile_options(App PRIVATE -Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# Output — bin/{Config}-{OS}-x86_64/App/, matching premake layout
# ---------------------------------------------------------------------------
set_property(TARGET App PROPERTY FOLDER "Projects")

set_target_properties(App PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>-${CMAKE_SYSTEM_NAME}-x86_64/App"
)
