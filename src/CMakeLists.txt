# **************************************************************************************************
# External dependencies

if(NOT SPECTRUM_DEBUG)
    # Audio Library for parsing files (FFMPEG)
    pkg_search_module(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_search_module(AVFILTER REQUIRED IMPORTED_TARGET libavfilter)
    pkg_search_module(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_search_module(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
    pkg_search_module(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

    # Audio Library for Playback (using direclty ALSA)
    pkg_search_module(ALSA REQUIRED IMPORTED_TARGET alsa)

    # DSP Processing (FFTW3)
    pkg_search_module(FFTW REQUIRED IMPORTED_TARGET fftw3)
endif()

# GUI Library (FTXUI)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
    GIT_TAG 9babfea36b1def5791b4c5446fc47dafa79a0289)

FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
    FetchContent_Populate(ftxui)
    add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Enable multithreading
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# **************************************************************************************************
# Create library

add_library(spectrum_lib OBJECT)
target_sources(
    spectrum_lib
    PRIVATE # audio
            audio/command.cc
            audio/player.cc
            # middleware
            middleware/media_controller.cc
            # model
            model/audio_filter.cc
            model/block_identifier.cc
            model/bar_animation.cc
            model/song.cc
            # view
            view/base/block.cc
            view/base/custom_event.cc
            view/base/terminal.cc
            view/block/file_info.cc
            view/block/list_directory.cc
            view/block/media_player.cc
            view/block/tab_item/audio_equalizer.cc
            view/block/tab_item/spectrum_visualizer.cc
            view/block/tab_viewer.cc
            view/element/button.cc
            view/element/error_dialog.cc
            view/element/help.cc
            view/element/tab_item.cc
            # logger
            util/logger.cc
            util/sink.cc)

target_include_directories(spectrum_lib PUBLIC ${CMAKE_SOURCE_DIR}/include
                                               $<BUILD_INTERFACE:${ftxui_SOURCE_DIR}/include>)

target_link_libraries(
    spectrum_lib
    PRIVATE ftxui::screen ftxui::dom ftxui::component
    PRIVATE Threads::Threads)

check_coverage(spectrum_lib)

# **************************************************************************************************
# Build with external dependencies (debug mode disabled)

if(NOT SPECTRUM_DEBUG)
    message(STATUS "Enabling external dependencies...")
    target_sources(
        spectrum_lib PRIVATE # audio
                             audio/driver/alsa.cc audio/driver/ffmpeg.cc audio/driver/fftw.cc)

    target_include_directories(
        spectrum_lib
        PUBLIC ${ALSA_INCLUDE_DIRS}
               ${AVCODEC_INCLUDE_DIRS}
               ${AVFILTER_INCLUDE_DIRS}
               ${AVFORMAT_INCLUDE_DIRS}
               ${AVUTIL_INCLUDE_DIRS}
               ${SWRESAMPLE_INCLUDE_DIRS}
               ${FFTW_INCLUDE_DIRS})

    target_link_libraries(
        spectrum_lib
        PRIVATE
        INTERFACE PkgConfig::AVCODEC
        INTERFACE PkgConfig::AVFILTER
        INTERFACE PkgConfig::AVFORMAT
        INTERFACE PkgConfig::AVUTIL
        INTERFACE PkgConfig::SWRESAMPLE
        PRIVATE
        INTERFACE PkgConfig::ALSA
        PRIVATE
        INTERFACE PkgConfig::FFTW)
endif()

# **************************************************************************************************
# Create executable

add_executable(spectrum)
target_sources(spectrum PRIVATE main.cc)
target_link_libraries(spectrum PRIVATE spectrum_lib)
target_compile_options(
    spectrum
    PRIVATE -Wall
            -Wextra
            -Wshadow
            -Wconversion
            -Werror
            -Wno-sign-compare
            -Wfatal-errors)

# **************************************************************************************************
# Install executable

if(ENABLE_INSTALL)
    install(TARGETS spectrum DESTINATION ${CMAKE_INSTALL_BIN})
endif()
