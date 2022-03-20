# ##################################################################################################
# External dependencies
# ##################################################################################################
pkg_check_modules(NCURSES REQUIRED ncurses)
pkg_check_modules(FFTW REQUIRED fftw3)

# **************************************************************************************************
# Compile flags for release OR debug
# IF(RELEASE)
#     SET(CMAKE_CXX_FLAGS "-Wall -Werror -o2")
# ELSE()
#     SET(CMAKE_CXX_FLAGS "-g -Wall -Werror")
# ENDIF()

# # FFTW
# pkg_search_module(FFTW REQUIRED fftw3 IMPORTED_TARGET)
# include_directories(PkgConfig::FFTW)
# link_libraries     (PkgConfig::FFTW)

# # NCURSES
# find_package(Curses REQUIRED)
# include_directories(${CURSES_INCLUDE_DIR})

# **************************************************************************************************
# Create library
add_library(spectrum-lib OBJECT)
target_sources(spectrum-lib
               PRIVATE
               ui/base/terminal.cc
               ui/base/block.cc
               ui/block/list_directory.cc
               ui/block/file_info.cc
               sound/wave.cc)

target_include_directories(spectrum-lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(spectrum-lib PUBLIC ncurses fftw3)

# **************************************************************************************************
# Create executable
add_executable(spectrum)
target_sources(spectrum PRIVATE main.cc)
target_link_libraries(spectrum PRIVATE spectrum-lib)
# add -Wextra
target_compile_options(spectrum
                       PRIVATE -Wall
                               -Werror
                               -Wno-sign-compare)
