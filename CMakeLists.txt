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

# Add compiler options for code coverage
function(check_coverage library)
    if(ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
            target_compile_options(${library} INTERFACE --coverage -O0 -g)
            target_link_libraries(${library} INTERFACE --coverage)
        endif()
    endif()
endfunction()

# Build options
option(SPECTRUM_DEBUG "Set to ON to build without external dependencies (ALSA, FFmpeg, FFTW3)" OFF)
option(ENABLE_TESTS "Set to ON to build executable for unit testing" OFF)
option(ENABLE_COVERAGE "Set to ON to build tests with coverage" OFF)
option(ENABLE_INSTALL "Generate the install target" ON)

if(SPECTRUM_DEBUG)
    message(STATUS "Enabling debug mode...")
    add_definitions(-DSPECTRUM_DEBUG)
endif()

# Build application
add_subdirectory(src)

if(ENABLE_TESTS AND NOT SPECTRUM_DEBUG)
    message(STATUS "Enabling tests...")
    add_definitions(-DENABLE_TESTS)
    add_subdirectory(test)
endif()
