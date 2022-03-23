cmake_minimum_required(VERSION 3.18)
project(spectrum)

set(CMAKE_CXX_STANDARD 17)

# Export compilation database for external tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use pkg-config and FetchContent modules for CMake build
include(FindPkgConfig)
include(FetchContent)

# Subdirectories
add_subdirectory(src)
add_subdirectory(test)
