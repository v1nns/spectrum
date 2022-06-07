# **************************************************************************************************
# External dependencies

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
    GIT_TAG v2.0.0)

FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
    FetchContent_Populate(ftxui)
    add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

pkg_check_modules(FFTW REQUIRED fftw3)

# **************************************************************************************************
# For testing

add_executable(dummy)
target_sources(dummy PRIVATE dummy.cc)
target_link_libraries(
    dummy
    PRIVATE fftw3
    PRIVATE ftxui::screen
    PRIVATE ftxui::dom
    PRIVATE ftxui::component)

target_compile_options(dummy PRIVATE -Werror -Wno-sign-compare)
