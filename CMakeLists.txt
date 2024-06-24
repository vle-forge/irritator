cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(irritator VERSION 0.1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

option(WITH_GUI "Build the graphical user interface [default: ON]" ON)
option(WITH_DEBUG "enable maximium debug code. [default: ON]" ON)

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

file(COPY examples/
  DESTINATION
  ${CMAKE_BINARY_DIR}/app/gui/share/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}/components)

install(DIRECTORY examples/
  DESTINATION
  ${CMAKE_INSTALL_FULL_DATAROOTDIR}/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}/components)

enable_testing()
add_subdirectory(lib)
add_subdirectory(app)
