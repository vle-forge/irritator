cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(libirritator VERSION 0.1.0 LANGUAGES CXX)

option(WITH_DEBUG "enable maximium debug code. [default: ON]" ON)

set(public_irritator_header
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/core.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/error.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/examples.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/ext.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/file.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/format.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/helpers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/io.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/macros.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/modeling-helpers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/modeling.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/observation.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/thread.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/irritator/timeline.hpp)

add_library(libirritator INTERFACE)
target_sources(libirritator INTERFACE
  "$<BUILD_INTERFACE:${public_irritator_header}>")

target_include_directories(libirritator INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>)

target_include_directories(libirritator SYSTEM INTERFACE
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>)

target_compile_options(libirritator INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -pipe -march=native
        $<$<CONFIG:Debug>:-O0 -g -Wall -Wextra>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS_EQUAL:${MSVC_TOOLSET_VERSION},142>>:
        /EHsc /bigobj /Zc:__cplusplus /wd5030 /std:c++latest
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_GREATER:${MSVC_TOOLSET_VERSION},142>>:
        /EHsc /bigobj /Zc:__cplusplus /wd5030 /std:c++20
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/IrritatorConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/IrritatorConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/IrritatorConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)

install(TARGETS libirritator
  EXPORT libirritator-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/IrritatorConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/IrritatorConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)

configure_file(irritator.pc.in
  "${CMAKE_BINARY_DIR}/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.pc"
  @ONLY)

install(FILES
  ${CMAKE_BINARY_DIR}/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(DIRECTORY include/irritator
  DESTINATION
  ${CMAKE_INSTALL_INCLUDEDIR}/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

function(irritator_add_test test_name)
  add_executable(${test_name} ${ARGN} "src/archiver.cpp;src/dot-parser.hpp;src/dot-parser.cpp;src/error.cpp;src/external_source.cpp;src/file.cpp;src/graph-observer.cpp;src/grid-observer.cpp;src/hsm.cpp;src/io.cpp;src/json.cpp;src/memory-resource.cpp;src/modeling.cpp;src/modeling-generic.cpp;src/modeling-graph.cpp;src/modeling-grid.cpp;src/modeling-internal.cpp;src/parameter.cpp;src/project.cpp;src/timeline.cpp;src/variable-observer.cpp")

  set_target_properties(${test_name} PROPERTIES
    COMPILE_DEFINITIONS EXAMPLES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/test\"
    CXX_STANDARD 20)

  target_compile_options(${test_name} BEFORE
    PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -pipe -march=native
        $<$<CONFIG:Debug>:-O0 -g -Wall -Wextra>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS_EQUAL:${MSVC_TOOLSET_VERSION},142>>:
        /EHsc /bigobj /Zc:__cplusplus /wd5030 /std:c++latest
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_GREATER:${MSVC_TOOLSET_VERSION},142>>:
        /EHsc /bigobj /Zc:__cplusplus /wd5030 /std:c++20
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>)

  target_compile_definitions(${test_name}
    PRIVATE
    $<$<BOOL:${WITH_DEBUG}>:IRRITATOR_ENABLE_DEBUG>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:BOOST_UT_DISABLE_MODULE>)

  target_include_directories(${test_name} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${CMAKE_SOURCE_DIR}/external/hayai/src
    $<INSTALL_INTERFACE:include>
    PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR})

  target_link_libraries(${test_name}
    $<$<AND:$<PLATFORM_ID:Linux>,$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>
    $<$<AND:$<PLATFORM_ID:Linux>,$<CXX_COMPILER_ID:Clang>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:c++fs>
    $<$<AND:$<PLATFORM_ID:Darwin>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:c++fs>
    threads
    libirritator)

  add_test(${test_name} ${test_name})
endfunction()

irritator_add_test(test-container test/containers.cpp)
irritator_add_test(test-api test/public-api.cpp)
irritator_add_test(test-mod-to-sim test/mod-to-sim.cpp)
irritator_add_test(test-thread test/threading.cpp)
irritator_add_test(test-simulations test/simulations.cpp)
irritator_add_test(test-dot-parser test/dot-parser-test.cpp)
