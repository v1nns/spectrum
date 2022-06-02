# **************************************************************************************************
# External dependencies

FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v2.0.0
)

FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
  FetchContent_Populate(ftxui)
  add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

pkg_check_modules(ALSA REQUIRED alsa)

# pkg_check_modules(FFTW REQUIRED fftw3)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# **************************************************************************************************
# Create library

add_library(spectrum-lib OBJECT)
target_sources(spectrum-lib
               PRIVATE
               # audio
               audio/player.cc
               audio/driver/alsa.cc
               # controller
               controller/media.cc
               # model
               model/song.cc
               model/wave.cc
               # view
               view/base/terminal.cc
               view/base/block_event.cc
               view/base/block.cc
               view/block/list_directory.cc
               view/block/file_info.cc
               view/block/audio_player.cc
               view/element/button.cc)

target_include_directories(spectrum-lib PUBLIC ${CMAKE_SOURCE_DIR}/include
                                        $<BUILD_INTERFACE:${ftxui_SOURCE_DIR}/include>
                                        $<BUILD_INTERFACE:${expected_SOURCE_DIR}/include>
                                        ${ALSA_INCLUDE_DIRS})

target_link_libraries(spectrum-lib
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
  PRIVATE ${ALSA_LIBRARIES}
  PRIVATE Threads::Threads
)

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

# **************************************************************************************************
# For prototyping
add_subdirectory(prototype)
