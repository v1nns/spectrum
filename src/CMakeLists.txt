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

# pkg_check_modules(FFTW REQUIRED fftw3)

# **************************************************************************************************
# Create library

add_library(spectrum-lib OBJECT)
target_sources(spectrum-lib
               PRIVATE
               ui/base/terminal.cc
               ui/block/list_directory.cc
               ui/block/file_info.cc
               sound/wave.cc)

target_include_directories(spectrum-lib PUBLIC ${CMAKE_SOURCE_DIR}/include $<BUILD_INTERFACE:${ftxui_SOURCE_DIR}/include>)

target_link_libraries(spectrum-lib
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
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
