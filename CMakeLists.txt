cmake_minimum_required(VERSION 3.30)
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
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES
                                               ".*Clang")
      # Instrument sources from the target itself, and link gcov also on targets that depend on it
      target_compile_options(${library} PRIVATE --coverage -O0 -g)
      target_link_libraries(${library} PUBLIC --coverage)
    endif()
  endif()
endfunction()

# Build options
option(SPECTRUM_DEBUG
       "Set to ON to build without external dependencies (ALSA, FFmpeg, FFTW3)"
       OFF)
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

# Create package (.deb) containing only the executable, i.e. without files installed by FTXUI
if(ENABLE_INSTALL)
  # Version from git tag, when available (e.g. "v1.0.0" results in "1.0.0")
  execute_process(
    COMMAND git describe --tags --match "v[0-9]*"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE SPECTRUM_PACKAGE_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

  string(REGEX REPLACE "^v" "" SPECTRUM_PACKAGE_VERSION "${SPECTRUM_PACKAGE_VERSION}")

  if(NOT SPECTRUM_PACKAGE_VERSION)
    set(SPECTRUM_PACKAGE_VERSION "0.0.0")
  endif()

  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_NAME "spectrum")
  set(CPACK_PACKAGE_VERSION "${SPECTRUM_PACKAGE_VERSION}")
  set(CPACK_PACKAGE_CONTACT "Vinicius Moura Longaray <vmouralongaray@gmail.com>")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
      "Simple and intuitive console-based music player with audio spectrum visualizer")
  set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/v1nns/spectrum")
  set(CPACK_STRIP_FILES ON)

  # Pack a single component, using the package name without any suffix
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  set(CPACK_COMPONENTS_ALL runtime)
  set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)

  # Dependencies are found from shared libraries linked to executable, while yt-dlp is optional
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_SUGGESTS "yt-dlp")

  include(CPack)
endif()
