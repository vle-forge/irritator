cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(libirritator VERSION 0.1.0 LANGUAGES CXX)

option(WITH_DEBUG "enable maximium debug code. [default: ON]" ON)

set(public_irritator_header
    include/irritator/core.hpp)

set(private_irritator_source
    src/adder.cpp
    src/counter.cpp
    src/generator.cpp)

add_library(libirritator ${public_irritator_header} ${private_irritator_source})

option(BUILD_SHARED_LIBS "Build shared library" OFF)
include(GenerateExportHeader)
generate_export_header(libirritator
    EXPORT_MACRO_NAME BX_EXPORT
    NO_EXPORT_MACRO_NAME BX_NO_EXPORT
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/irritator/export)

target_compile_options(libirritator
  PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
      -pipe -march=native
      $<$<CONFIG:Debug>:-O0 -ggdb3 -Wall -Wextra>>
  $<$<CXX_COMPILER_ID:MSVC>:
      /EHsc /bigobj /Zc:__cplusplus /std:c++latest
      $<$<CONFIG:Debug>:/Od /W3 /Zi>>)

target_include_directories(libirritator PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/>)

target_link_libraries(libirritator
  PRIVATE
  threads)

set_target_properties(libirritator PROPERTIES
  OUTPUT_NAME "irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  CXX_STANDARD 20)

target_compile_definitions(libirritator
  PRIVATE
  $<$<BOOL:${WITH_DEBUG}>:IRRITATOR_ENABLE_DEBUG>
  $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
  $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_WARNINGS>
  VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  VERSION_MINOR=${PROJECT_VERSION_MINOR}
  VERSION_PATCH=${PROJECT_VERSION_PATCH})

install(TARGETS libirritator
  EXPORT libirritator-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(IRRITATOR_DEPENDENCIES "")
configure_file(irritator.pc.in
  "${CMAKE_BINARY_DIR}/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.pc"
  @ONLY)

install(FILES
  ${CMAKE_BINARY_DIR}/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(DIRECTORY include/irritator
  DESTINATION
  ${CMAKE_INSTALL_INCLUDEDIR}/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/irritator/export
  DESTINATION
  ${CMAKE_INSTALL_INCLUDEDIR}/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}/irritator)

install(EXPORT libirritator-targets
    FILE libirritator-targets.cmake
    NAMESPACE libirritator::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/irritator-${VLE_ABI})

# This makes the project importable from the build directory
export(TARGETS libirritator FILE LibirritatorConfig.cmake)

function(irritator_add_test test_name)
  add_executable(${test_name} ${ARGN})

  set_target_properties(${test_name} PROPERTIES
    COMPILE_DEFINITIONS EXAMPLES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/test\"
    CXX_STANDARD 20)

  target_compile_options(${test_name} BEFORE
    PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -pipe -march=native
        $<$<CONFIG:Debug>:-Og -Wall -Wextra>>
    $<$<CXX_COMPILER_ID:MSVC>:
        /EHsc /bigobj /Zc:__cplusplus /std:c++latest
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>)

  target_compile_definitions(${test_name}
    PRIVATE
    $<$<BOOL:${WITH_DEBUG}>:IRRITATOR_ENABLE_DEBUG>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_WARNINGS>)

  target_include_directories(${test_name} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${CMAKE_SOURCE_DIR}/external/hayai/src
    $<INSTALL_INTERFACE:include>
    PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR})

  target_link_libraries(${test_name} libirritator threads)

  add_test(${test_name} ${test_name})
endfunction()

function(irritator_add_benchmark test_name)
  add_executable(${test_name} ${ARGN})

  set_target_properties(${test_name} PROPERTIES
    COMPILE_DEFINITIONS EXAMPLES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/test\"
    CXX_STANDARD 20)

  target_compile_options(${test_name} BEFORE
    PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -pipe -march=native
        $<$<CONFIG:Debug>:-Og -Wall -Wextra>>
    $<$<CXX_COMPILER_ID:MSVC>:
        /EHsc /bigobj /Zc:__cplusplus /std:c++latest
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>)

  target_compile_definitions(${test_name}
    PRIVATE
    $<$<BOOL:${WITH_DEBUG}>:IRRITATOR_ENABLE_DEBUG>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_WARNINGS>)

  target_include_directories(${test_name} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${CMAKE_SOURCE_DIR}/external/hayai/src
    $<INSTALL_INTERFACE:include>
    PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR})

  target_link_libraries(${test_name} libirritator threads)
endfunction()

if (NOT BUILD_SHARED_LIBS)
  irritator_add_test(test-api test/public-api.cpp)
  irritator_add_test(simulations test/simulations.cpp)
  irritator_add_test(auditory test/auditory.cpp)

  irritator_add_benchmark(benchmark_exactitude_aqss benchmark/benchmark_exactitude_aqss.cpp)
  irritator_add_benchmark(benchmark_exactitude_qss1 benchmark/benchmark_exactitude_qss1.cpp)
  irritator_add_benchmark(benchmark_exactitude_qss2 benchmark/benchmark_exactitude_qss2.cpp)

  irritator_add_benchmark(benchmark_timing_qss1 benchmark/benchmark_timing_qss1.cpp)
  irritator_add_benchmark(benchmark_timing_qss2 benchmark/benchmark_timing_qss2.cpp)
  irritator_add_benchmark(benchmark_timing_aqss benchmark/benchmark_timing_aqss.cpp)
endif ()
