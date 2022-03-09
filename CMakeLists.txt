cmake_minimum_required(VERSION 3.18)
PROJECT(spectrum)

# Export compilation database for external tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Package dependency
find_package(PkgConfig REQUIRED)

# NCURSES
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

# Compile flags for release OR debug
IF(RELEASE)
    SET(CMAKE_CXX_FLAGS "-Wall -Werror -o2 -std=c++17")
ELSE()
    SET(CMAKE_CXX_FLAGS "-g -Wall -Werror -std=c++17")
ENDIF()

include_directories(include)

ADD_EXECUTABLE(spectrum src/main.cc
                        src/ui/base/terminal.cc
                        src/ui/base/block.cc
                        src/ui/block/list_dir.cc
                        src/ui/block/file_info.cc
                        src/sound/wave.cc)

target_link_libraries(spectrum ${CURSES_LIBRARY})
