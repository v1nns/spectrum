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

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR}
                   EXCLUDE_FROM_ALL)
endif()

# DSP Processing (FFTW3)
pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3)



# **************************************************************************************************
# Create executable

add_executable(test)
target_sources(test PRIVATE block_file_info.cc block_list_directory.cc
                            block_media_player.cc audio_player.cc driver_fftw.cc)

target_link_libraries(test PRIVATE gtest gmock gtest_main spectrum-lib)

target_include_directories(test PRIVATE ${CMAKE_SOURCE_DIR}/include
                                        ${CMAKE_SOURCE_DIR}/test)

target_compile_options(test PRIVATE -Wall -Werror -Wno-sign-compare)
