cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(irritator VERSION 0.1.0.0 LANGUAGES CXX)

set(CMAKE_MSVC_DEPENDENCY_FILE_FLAG TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include(GNUInstallDirs)

option(WITH_GUI "Build the graphical user interface [default: ON]" ON)
option(WITH_NATIVE_ARCH "Enable the -march=native flags on GCC/Cang [default: ON]" ON)
option(WITH_MEMORY_LOG
  "Log each allocation in irt-mem.txt with the debug code [default: OFF]" OFF)

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

if (WITH_MEMORY_LOG)
  add_compile_definitions(IRRITATOR_ENABLE_MEMORY_LOG)
endif ()

find_package(Threads REQUIRED)
add_library(threads INTERFACE IMPORTED)
set_property(TARGET threads PROPERTY
  INTERFACE_LINK_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

find_package(Fontconfig)

# Each MSVC compiler takes several gigabytes on the largest sources: Ninja
# runs one per logical core and exhausts the memory. The pools limit the
# compilers to half the cores and to one per 4 GiB of memory.
if (MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
  cmake_host_system_information(RESULT irritator_cores
    QUERY NUMBER_OF_LOGICAL_CORES)
  cmake_host_system_information(RESULT irritator_memory
    QUERY TOTAL_PHYSICAL_MEMORY)
  math(EXPR irritator_jobs "${irritator_cores} / 2")
  math(EXPR irritator_jobs_memory "${irritator_memory} / 4096")
  if (irritator_jobs_memory LESS irritator_jobs)
    set(irritator_jobs ${irritator_jobs_memory})
  endif ()
  if (irritator_jobs LESS 1)
    set(irritator_jobs 1)
  endif ()

  set(IRRITATOR_COMPILE_JOBS ${irritator_jobs} CACHE STRING
    "Maximum number of parallel MSVC compilations with Ninja")
  set(IRRITATOR_LINK_JOBS 2 CACHE STRING
    "Maximum number of parallel MSVC links with Ninja")
  set_property(GLOBAL APPEND PROPERTY JOB_POOLS
    irritator_compile=${IRRITATOR_COMPILE_JOBS}
    irritator_link=${IRRITATOR_LINK_JOBS})
  set(CMAKE_JOB_POOL_COMPILE irritator_compile)
  set(CMAKE_JOB_POOL_LINK irritator_link)
endif ()

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

# The digital circuits written by test-digital-circuits, once the tests ran.
install(DIRECTORY
  ${CMAKE_BINARY_DIR}/app/gui/share/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}/components/digital
  DESTINATION
  ${CMAKE_INSTALL_FULL_DATAROOTDIR}/irritator-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}/components
  OPTIONAL)

enable_testing()
add_subdirectory(lib)
add_subdirectory(app)
add_subdirectory(tools/digital)
add_subdirectory(test)
