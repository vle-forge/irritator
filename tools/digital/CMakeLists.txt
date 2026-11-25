cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(irritator-digital VERSION 0.1.0 LANGUAGES CXX)

add_library(libirritator-digital STATIC digital.hpp digital.cpp)

target_include_directories(libirritator-digital PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(libirritator-digital PUBLIC libirritator)

set_target_properties(libirritator-digital PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON)

target_compile_options(libirritator-digital PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
      $<IF:$<BOOL:${WITH_NATIVE_ARCH}>,-pipe -march=native,-pipe>
      $<$<CONFIG:Debug>:-O0 -g -Wall -Wextra>>
  $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS_EQUAL:${MSVC_TOOLSET_VERSION},142>>:
      /utf-8 /EHsc /bigobj /Zc:__cplusplus /wd5030 /wd4251 /std:c++latest
      $<$<CONFIG:Debug>:/Od /W3 /Zi>>
  $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_GREATER:${MSVC_TOOLSET_VERSION},142>>:
      /utf-8 /EHsc /bigobj /Zc:__cplusplus /wd5030 /wd4251 /std:c++20
      $<$<CONFIG:Debug>:/Od /W3 /Zi>>)

target_compile_definitions(libirritator-digital PRIVATE
  $<$<BOOL:${IRRITATOR_DEBUG_COND}>:IRRITATOR_ENABLE_DEBUG>
  $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
  $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_WARNINGS>)

add_executable(irritator-digital main.cpp)
target_link_libraries(irritator-digital PRIVATE libirritator-digital)
set_target_properties(irritator-digital PROPERTIES CXX_STANDARD 20)
