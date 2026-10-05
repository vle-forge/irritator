cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(irritator VERSION 0.1.0.0 LANGUAGES CXX)

set(CMAKE_MSVC_DEPENDENCY_FILE_FLAG TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(GNUInstallDirs)

option(WITH_GUI "Build the graphical user interface [default: ON]" ON)
option(WITH_NATIVE_ARCH "Enable the -march=native flags on GCC/Cang [default: ON]" ON)

set(WITH_DEBUG "AUTO" CACHE STRING
    "Enable maximum debug code: ON, OFF or AUTO (ON only for the Debug configuration)")
set_property(CACHE WITH_DEBUG PROPERTY STRINGS AUTO ON OFF)

if (WITH_DEBUG STREQUAL "AUTO")
  set(IRRITATOR_DEBUG_COND "$<CONFIG:Debug>")
elseif (WITH_DEBUG)
  set(IRRITATOR_DEBUG_COND "1")
else ()
  set(IRRITATOR_DEBUG_COND "0")
endif ()

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

file(COPY examples/
  DESTINATION
  ${CMAKE_BINARY_DIR}/app/gui/share/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}/components)

install(DIRECTORY examples/
  DESTINATION
  ${CMAKE_INSTALL_FULL_DATAROOTDIR}/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}/components)

enable_testing()
add_subdirectory(lib)
add_subdirectory(app)
add_subdirectory(test)
