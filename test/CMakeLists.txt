cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(irritator-cli VERSION 0.1.0.0 LANGUAGES CXX)

function(irritator_add_test test_name)
  add_executable(${test_name} utils.hpp;${ARGN})

  set_target_properties(${test_name} PROPERTIES
    COMPILE_DEFINITIONS EXAMPLES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"
    CXX_STANDARD 20)

  target_compile_options(${test_name} BEFORE
    PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -pipe -march=native
        $<$<CONFIG:Debug>:-O0 -g -Wall -Wextra>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS_EQUAL:${MSVC_TOOLSET_VERSION},142>>:
        /utf-8 /EHsc /bigobj /Zc:__cplusplus /wd4251 /wd5030 /std:c++latest
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_GREATER:${MSVC_TOOLSET_VERSION},142>>:
        /utf-8 /EHsc /bigobj /Zc:__cplusplus /wd4251 /wd5030 /std:c++20
        $<$<CONFIG:Debug>:/Od /W3 /Zi>>)

  target_compile_definitions(${test_name}
    PRIVATE
    $<$<BOOL:${WITH_DEBUG}>:IRRITATOR_ENABLE_DEBUG>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:MSVC>:BOOST_UT_DISABLE_MODULE>
    IRT_DATAROOTDIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}")

  target_include_directories(${test_name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
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

irritator_add_test(test-container containers.cpp)
irritator_add_test(test-api public-api.cpp)
irritator_add_test(test-mod-to-sim mod-to-sim.cpp)
irritator_add_test(test-thread threading.cpp)
irritator_add_test(test-simulations simulations.cpp)
irritator_add_test(test-dot-parser dot-parser-test.cpp)
irritator_add_test(test-qss qss.cpp)
irritator_add_test(test-dama dama.cpp)
