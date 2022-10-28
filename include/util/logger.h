/**
 * \file
 * \brief  Class for logging
 */

#ifndef INCLUDE_UTIL_LOGGER_H_
#define INCLUDE_UTIL_LOGGER_H_

#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

namespace logger {

//! Forward declaration
class Logger;

/**
 * @brief Get unique instance of Logger (singleton pattern)
 * @return Logger instance
 */
inline Logger& get_logger() {
  // TODO: make it configurable via command line argument
  static std::unique_ptr<Logger> singleton = std::make_unique<Logger>("/tmp/teste.txt");
  return *singleton;
}

/**
 * @brief Get current timestamp in a formatted string
 * @return String containing timestamp
 */
std::string get_timestamp();

/* ********************************************************************************************** */

/**
 * @brief Responsible for message logging (thread-safe) to a defined filepath
 */
class Logger {
 public:
  // Remove default constructor
  Logger() = delete;

  /**
   * @brief Construct a new Logger object
   * @param filepath Full path for file to write log messages
   */
  Logger(const std::string& filepath);

  /**
   * @brief Destroy the Logger object
   */
  virtual ~Logger() = default;

 private:
  void OpenFileStream();
  void WriteToFile(const std::string& message);

 public:
  /**
   * @brief Concatenate all arguments into a single log message and write it to file
   * @tparam ...Args Splitted arguments
   * @param filename Current file name
   * @param line Current line number
   * @param ...args Arguments to build log message
   */
  template <typename... Args>
  void Log(const char* filename, int line, Args&&... args) {
    std::ostringstream ss;

    ss << "[" << get_timestamp() << "] ";
    ss << "[" << filename << ":" << line << "] ";
    (ss << ... << std::forward<Args>(args)) << "\n";

    WriteToFile(std::move(ss).str());
  }

 private:
  std::mutex mutex_;                                   //!< Control access for internal resources
  std::string filepath_;                               //!< Absolute path for log file
  std::ofstream file_;                                 //!< Output stream to operate on log file
  std::chrono::seconds reopen_interval_;               //!< Interval to reopen file
  std::chrono::system_clock::time_point last_reopen_;  //!< Last timestamp that file was (re)opened
};

}  // namespace logger

/* ---------------------------------------------------------------------------------------------- */
/*                                           PUBLIC API                                           */
/* ---------------------------------------------------------------------------------------------- */

//! Parse pre-processing macro to get only filename instead of absolute path
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

//! Macro to log messages (this was the only way found to append "filename:line" in the output)
#define LOG(...) logger::get_logger().Log(__FILENAME__, __LINE__, __VA_ARGS__)

//! Macro to log error messages
#define ERROR(...) logger::get_logger().Log(__FILENAME__, __LINE__, "ERROR: ", __VA_ARGS__)

#endif  // INCLUDE_UTIL_LOGGER_H_