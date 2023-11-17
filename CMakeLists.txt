cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(irritator VERSION 0.1.0.0 LANGUAGES CXX)

# Use the GNU standard installation directories. See
# https://cmake.org/cmake/help/v3.0/module/GNUInstallDirs.html
include(GNUInstallDirs)

option(WITH_GUI "Build the graphical user interface [default: ON]" ON)

find_package(Threads REQUIRED)
add_library(threads INTERFACE IMPORTED)
set_property(TARGET threads PROPERTY
  INTERFACE_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

find_package(Fontconfig)

add_definitions(-DFMT_HEADER_ONLY)
include_directories(${CMAKE_SOURCE_DIR}/external/leaf/include)
include_directories(${CMAKE_SOURCE_DIR}/external/fmt/include)
include_directories(${CMAKE_SOURCE_DIR}/external/ut/include)
include_directories(${CMAKE_SOURCE_DIR}/external/rapidjson/include)
include_directories(${CMAKE_SOURCE_DIR}/external/random123/include)

enable_testing()
add_subdirectory(lib)
add_subdirectory(app)
