cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(irritator-cli VERSION 0.1.0.0 LANGUAGES CXX)

add_executable(app src/main.cpp)

target_link_libraries(app
    PRIVATE
    $<$<AND:$<PLATFORM_ID:Linux>,$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>
    $<$<AND:$<PLATFORM_ID:Linux>,$<CXX_COMPILER_ID:Clang>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:c++fs>
    $<$<AND:$<PLATFORM_ID:Darwin>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:c++fs>
    threads
    libirritator)

set_target_properties(app PROPERTIES
  OUTPUT_NAME "irritator-cli-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
  CXX_STANDARD 20)

target_compile_options(app
  PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -pipe -march=native
        $<$<CONFIG:Debug>:-O0 -g -Wall -Wextra>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS_EQUAL:${MSVC_TOOLSET_VERSION},142>>:
        /utf-8 /EHsc /bigobj /Zc:__cplusplus /wd5030 /std:c++latest
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_GREATER:${MSVC_TOOLSET_VERSION},142>>:
        /utf-8 /EHsc /bigobj /Zc:__cplusplus /wd5030 /std:c++20
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>)

target_compile_definitions(app
  PRIVATE
  $<$<BOOL:${WITH_DEBUG}>:IRRITATOR_ENABLE_DEBUG>
  $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
  $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_WARNINGS>
  VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  VERSION_MINOR=${PROJECT_VERSION_MINOR}
  VERSION_PATCH=${PROJECT_VERSION_PATCH}
  VERSION_TWEAK=${PROJECT_VERSION_TWEAK}
  IRT_DATAROOTDIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}")

target_include_directories(app
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/lib/include>
  $<INSTALL_INTERFACE:include>
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/../../external/imgui/backends
  ${PROJECT_SOURCE_DIR}/../../external/imgui
  ${PROJECT_SOURCE_DIR}/../../external/imgui/examples/libs/gl3w)

install(TARGETS app
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if (WITH_GUI)
    add_subdirectory(gui)
endif ()
