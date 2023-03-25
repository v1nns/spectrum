cmake_minimum_required(VERSION 3.18)
project(spectrum)

set(CMAKE_CXX_STANDARD 17)

# Export compilation database for external tools
set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE INTERNAL "")

# Use pkg-config and FetchContent modules for CMake build
find_package(PkgConfig REQUIRED)
include(FetchContent)

# Flag options
option(SPECTRUM_DEBUG "Set to ON to disable build with external dependencies" OFF)

if (SPECTRUM_DEBUG)
  MESSAGE(STATUS "SPECTRUM_DEBUG")
  add_definitions(-DSPECTRUM_DEBUG)
endif()

# Subdirectories
add_subdirectory(src)
add_subdirectory(test)
