# **************************************************************************************************
# External dependencies

# Audio Library for parsing files (FFMPEG)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

# Audio Library for Playback (using direclty ALSA)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)

# DSP Processing (FFTW3) pkg_check_modules(FFTW REQUIRED fftw3)

# GUI Library (FTXUI)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
    GIT_TAG v2.0.0)

FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
    FetchContent_Populate(ftxui)
    add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Multithreading
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# **************************************************************************************************
# Create library

add_library(spectrum-lib OBJECT)
target_sources(
    spectrum-lib
    PRIVATE # audio
            audio/driver/alsa.cc
            audio/driver/ffmpeg.cc
            audio/player.cc
            # controller
            controller/media.cc
            # model
            model/song.cc
            # view
            view/base/block.cc
            view/base/custom_event.cc
            view/base/terminal.cc
            view/block/list_directory.cc
            view/block/file_info.cc
            view/block/media_player.cc
            view/element/button.cc)

target_include_directories(
    spectrum-lib
    PUBLIC ${CMAKE_SOURCE_DIR}/include $<BUILD_INTERFACE:${ftxui_SOURCE_DIR}/include>
           $<BUILD_INTERFACE:${expected_SOURCE_DIR}/include> ${ALSA_INCLUDE_DIRS}
           ${AVCODEC_INCLUDE_DIRS})

target_link_libraries(
    spectrum-lib
    PRIVATE
    INTERFACE PkgConfig::AVCODEC
    INTERFACE PkgConfig::AVFORMAT
    INTERFACE PkgConfig::AVUTIL
    INTERFACE PkgConfig::SWRESAMPLE
    PRIVATE
    INTERFACE PkgConfig::ALSA
    PRIVATE ftxui::screen ftxui::dom ftxui::component
    PRIVATE Threads::Threads)

# **************************************************************************************************
# Create executable

add_executable(spectrum)
target_sources(spectrum PRIVATE main.cc)
target_link_libraries(spectrum PRIVATE spectrum-lib)
# add -Wextra
target_compile_options(spectrum PRIVATE -Wall -Werror -Wno-sign-compare -Wfatal-errors)

# **************************************************************************************************
# For prototyping
add_subdirectory(prototype)
