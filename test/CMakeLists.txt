if(SPECTRUM_DEBUG OR NOT ENABLE_TESTS)
    return()
endif()

# **************************************************************************************************
# External dependencies

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG release-1.11.0)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# **************************************************************************************************
# Create executable

enable_testing()

add_executable(test)
target_sources(
    test
    PRIVATE audio_player.cc
            block_file_info.cc
            block_list_directory.cc
            block_media_player.cc
            block_tab_viewer.cc
            driver_fftw.cc
            middleware_media_controller.cc)

target_link_libraries(test PRIVATE GTest::gtest GTest::gmock GTest::gtest_main spectrum_lib)

target_include_directories(test PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/test)

target_compile_options(test PRIVATE -Wall -Werror -Wno-sign-compare)

# Use a default path for ListDirectory block unit testing
target_compile_definitions(test PUBLIC LISTDIR_PATH="${CMAKE_SOURCE_DIR}/test")

check_coverage(test)

include(GoogleTest)
gtest_discover_tests(test DISCOVERY_TIMEOUT 30)
