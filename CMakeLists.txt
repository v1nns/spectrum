cmake_minimum_required(VERSION 3.18)
PROJECT(spectrum)

# Export compilation database for external tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Package dependency
find_package(PkgConfig REQUIRED)

# Compile flags for release OR debug
IF(RELEASE)
    SET(CMAKE_CXX_FLAGS "-Wall -Werror -o2 -std=c++17")
ELSE()
    SET(CMAKE_CXX_FLAGS "-g -Wall -Werror -std=c++17")
ENDIF()

include_directories(include)

ADD_EXECUTABLE(spectrum src/main.cc src/wave.cc)
