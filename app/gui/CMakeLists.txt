cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(irritator-gui VERSION 0.1.0.0 LANGUAGES CXX)

set(gui_sources
    application.cpp application.hpp component.cpp components-window.cpp
    dialog.cpp dialog.hpp editor.cpp editor.hpp hsm-editor.cpp imnodes.cpp
    imnodes.h imnodes_internal.h implot.cpp implot.h implot_internal.h
    implot_items.cpp implot_demo.cpp internal.hpp internal.cpp io.cpp
    modeling-editor.cpp notification.cpp output-editor.cpp 
    project-window.cpp simulation-editor.cpp simulation-component.cpp
    simulation-observation.cpp sources.cpp window-logger.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/archiver.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/json.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/file.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/modeling.cpp
    ${PROJECT_SOURCE_DIR}/../../lib/src/timeline.cpp
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

target_include_directories(irritator-gui
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/lib/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/../../external/imgui/backends
    ${PROJECT_SOURCE_DIR}/../../external/imgui
    ${PROJECT_SOURCE_DIR}/../../external/imgui/examples/libs/gl3w)

target_link_libraries(irritator-gui
    PRIVATE
    $<$<PLATFORM_ID:Linux>:GLEW::GLEW>
    $<$<PLATFORM_ID:Darwin>:GLEW::glew>
    $<$<AND:$<PLATFORM_ID:Linux>,$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>
    $<$<AND:$<PLATFORM_ID:Linux>,$<CXX_COMPILER_ID:Clang>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:c++fs>
    $<$<AND:$<PLATFORM_ID:Darwin>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:c++fs>
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
        $<$<CONFIG:Debug>:-Og -Wall -Wextra>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS_EQUAL:${MSVC_TOOLSET_VERSION},142>>:
        /EHsc /bigobj /Zc:__cplusplus /std:c++latest
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_GREATER:${MSVC_TOOLSET_VERSION},142>>:
        /EHsc /bigobj /Zc:__cplusplus /std:c++20
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>)

target_compile_definitions(irritator-gui
    PRIVATE
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
