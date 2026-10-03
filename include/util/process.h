/**
 * \file
 * \brief  Utilities to find and run external programs
 */

#ifndef INCLUDE_UTIL_PROCESS_H_
#define INCLUDE_UTIL_PROCESS_H_

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace util {

/**
 * @brief Result from running an external program
 */
struct ProcessResult {
  int exit_code = -1;      //!< Exit code (-1 if program did not exit normally, e.g. killed)
  bool timed_out = false;  //!< Program took longer than allowed and was killed
  std::string output;      //!< Content written to standard output
  std::string error;       //!< Content written to standard error
};

/**
 * @brief Search for an executable program in the directories from PATH environment variable
 * @param name Program name
 * @return Full path to program (or nothing, if not found)
 */
std::optional<std::filesystem::path> FindExecutable(const std::string& name);

/**
 * @brief Run an external program and wait until it finishes (killing it after timeout)
 * @param args Program name (searched in PATH) followed by its arguments
 * @param timeout Maximum time to wait for program to finish
 * @return Result from program (or nothing, if it could not be started)
 */
std::optional<ProcessResult> RunProcess(const std::vector<std::string>& args,
                                        std::chrono::milliseconds timeout);

}  // namespace util
#endif  // INCLUDE_UTIL_PROCESS_H_
