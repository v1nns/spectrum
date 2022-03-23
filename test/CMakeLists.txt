# **************************************************************************************************
# External dependencies

FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip)

  # For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# **************************************************************************************************
# Create executable

add_executable(test)
target_sources(test
               PRIVATE
               block.cc)

target_link_libraries(test
                      PRIVATE gtest
                              gtest_main
                              spectrum-lib)

target_include_directories(test
                           PRIVATE ${CMAKE_SOURCE_DIR}/include
                                   ${CMAKE_SOURCE_DIR}/test)

target_compile_options(test
                       PRIVATE -Wall
                               -Werror
                               -Wno-sign-compare
                               -Wno-dev)