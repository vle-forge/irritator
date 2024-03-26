cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(irritator-gui VERSION 0.1.0.0 LANGUAGES CXX)

set(gui_sources application.cpp application.hpp component.cpp
    component-editor.cpp component-model-selector.cpp component-selector.cpp
    data-window.cpp dialog.cpp dialog.hpp editor.cpp editor.hpp imnodes.cpp
    imnodes.h imnodes_internal.h implot.cpp implot.h implot_demo.cpp
    implot_internal.h implot_items.cpp internal.cpp internal.hpp io.cpp
    library-window.cpp memory-window.cpp notification.cpp output-editor.cpp
    plot-copy-widget.cpp plot-observation-widget.cpp project-window.cpp
    settings-window.cpp simulation-component.cpp simulation-editor.cpp
    task-window.cpp window-logger.cpp

    generic/modeling.cpp
    generic/simulation.cpp
    graph/modeling.cpp
    graph/simulation.cpp
    grid/modeling.cpp
    grid/observation.cpp
    grid/simulation.cpp
    hsm/modeling.cpp

    ${PROJECT_SOURCE_DIR}/../../lib/src/archiver.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/dot-parser.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/dot-parser.hpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/error.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/external_source.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/graph-observer.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/grid-observer.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/hsm.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/json.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/io.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/file.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/memory-resource.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/modeling.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/modeling-generic.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/modeling-graph.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/modeling-grid.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/modeling-internal.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/parameter.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/project.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/timeline.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/variable-observer.cpp
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imgui.h
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imgui_demo.cpp
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imgui_tables.cpp
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imgui_widgets.cpp
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imgui_internal.h
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imconfig.h
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imstb_rectpack.h
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imstb_textedit.h
    ${PROJECT_SOURCE_DIR}/../../external/imgui/imstb_truetype.h)

if (WIN32)
    list(APPEND gui_sources
        main-windows.cpp
        ${PROJECT_SOURCE_DIR}/../../external/imgui/backends/imgui_impl_win32.cpp
        ${PROJECT_SOURCE_DIR}/../../external/imgui/backends/imgui_impl_win32.h
        ${PROJECT_SOURCE_DIR}/../../external/imgui/backends/imgui_impl_dx12.h
        ${PROJECT_SOURCE_DIR}/../../external/imgui/backends/imgui_impl_dx12.cpp)
else ()
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)
    list(APPEND gui_sources
        main-unix.cpp
        ${PROJECT_SOURCE_DIR}/../../external/imgui/backends/imgui_impl_glfw.cpp
        ${PROJECT_SOURCE_DIR}/../../external/imgui/backends/imgui_impl_glfw.h
        ${PROJECT_SOURCE_DIR}/../../external/imgui/backends/imgui_impl_opengl3.cpp
        ${PROJECT_SOURCE_DIR}/../../external/imgui/backends/imgui_impl_opengl3.h)
endif ()

add_executable(irritator-gui ${gui_sources})

option(USE_TTF "Use true type font" TRUE)

target_include_directories(irritator-gui
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/lib/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/../../external/imgui/backends
    ${PROJECT_SOURCE_DIR}/../../external/imgui
    ${PROJECT_SOURCE_DIR}/../../external/imgui/examples/libs/gl3w
    $<$<AND:$<PLATFORM_ID:Linux>,$<BOOL:${USE_TTF}>,$<BOOL:${Fontconfig_FOUND}>>:${Fontconfig_INCLUDE_DIRS}>)

target_link_libraries(irritator-gui
    PRIVATE
    $<$<PLATFORM_ID:Linux>:GLEW::GLEW>
    $<$<PLATFORM_ID:Darwin>:GLEW::glew>
    $<$<AND:$<PLATFORM_ID:Linux>,$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>
    $<$<AND:$<PLATFORM_ID:Linux>,$<CXX_COMPILER_ID:Clang>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:c++fs>
    $<$<AND:$<PLATFORM_ID:Darwin>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:c++fs>
    $<$<AND:$<PLATFORM_ID:Linux>,$<BOOL:${USE_TTF}>,$<BOOL:${Fontconfig_FOUND}>>:${Fontconfig_LIBRARIES}>
    threads
    $<$<PLATFORM_ID:Linux>:dl>
    $<$<PLATFORM_ID:Linux>:OpenGL::GL>
    $<$<PLATFORM_ID:Linux>:glfw>
    $<$<PLATFORM_ID:Darwin>:OpenGL::GL>
    $<$<PLATFORM_ID:Darwin>:glfw>
    $<$<PLATFORM_ID:Windows>:d3d12>
    $<$<PLATFORM_ID:Windows>:dxguid>
    $<$<PLATFORM_ID:Windows>:dxgi>
    $<$<PLATFORM_ID:Windows>:d3dcompiler>
    libirritator)

target_compile_options(irritator-gui
  PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -pipe -march=native
        $<$<CONFIG:Debug>:-O0 -g -Wall -Wextra>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS_EQUAL:${MSVC_TOOLSET_VERSION},142>>:
        /EHsc /bigobj /Zc:__cplusplus /wd5030 /std:c++latest
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_GREATER:${MSVC_TOOLSET_VERSION},142>>:
        /EHsc /bigobj /Zc:__cplusplus /wd5030 /std:c++20
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>
    $<$<AND:$<PLATFORM_ID:Linux>,$<BOOL:${USE_TTF}>,$<BOOL:${Fontconfig_FOUND}>>:${Fontconfig_COMPILE_OPTIONS}>)

target_compile_definitions(irritator-gui
    PRIVATE
    $<$<AND:$<PLATFORM_ID:Linux>,$<BOOL:${USE_TTF}>,$<BOOL:${Fontconfig_FOUND}>>:IRRITATOR_USE_TTF>
    $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${USE_TTF}>>:IRRITATOR_USE_TTF>
    $<$<BOOL:${WITH_DEBUG}>:IRRITATOR_ENABLE_DEBUG>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_WARNINGS>
    IMGUI_USER_CONFIG="${PROJECT_SOURCE_DIR}/imconfig.h"
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH})

set_target_properties(irritator-gui PROPERTIES
    OUTPUT_NAME "irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    CXX_STANDARD 20)

install(TARGETS irritator-gui
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
