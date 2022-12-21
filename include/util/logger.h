/**
 * \file
 * \brief  Class for logging
 */

#ifndef INCLUDE_UTIL_LOGGER_H_
#define INCLUDE_UTIL_LOGGER_H_

#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace util {

/**
 * @brief Responsible for message logging (thread-safe) to a defined filepath
 */
class Logger {
 public:
  /**
   * @brief Get a new unique instance of Logger and enable logging to specified path
   * @param filepath Full path for file to write log messages
   * @return Logger unique instance
   */
  static Logger& Configure(const std::string& filepath);

  /**
   * @brief Construct a new Logger object
   */
  Logger();

  /**
   * @brief Destroy the Logger object
   */
  virtual ~Logger();

  //! Remove these
  Logger(const Logger& other) = delete;             // copy constructor
  Logger(Logger&& other) = delete;                  // move constructor
  Logger& operator=(const Logger& other) = delete;  // copy assignment
  Logger& operator=(Logger&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Utility
 private:
  virtual void OpenStream();
  virtual void CloseStream();
  virtual bool IsLoggingEnabled();

  void Write(const std::string& message);

  /* ******************************************************************************************** */
  //! Public API
 public:
  /**
   * @brief Concatenate all arguments into a single log message and write it to output stream
   * @tparam ...Args Splitted arguments
   * @param filename Current file name
   * @param line Current line number
   * @param ...args Arguments to build log message
   */
  template <typename... Args>
  void Log(const char* filename, int line, Args&&... args) {
    if (!IsLoggingEnabled()) return;

    // Otherwise, build log message and write it to file
    std::ostringstream ss;

    ss << "[" << std::hex << std::this_thread::get_id() << std::dec << "] ";
    ss << "[" << filename << ":" << line << "] ";
    (ss << ... << std::forward<Args>(args)) << "\n";

    Write(std::move(ss).str());
  }

  /* ******************************************************************************************** */
  //! Variables
 protected:
  std::mutex mutex_;                          //!< Control access for internal resources
  std::shared_ptr<std::ostream> out_stream_;  //!< Output stream to write logs
};

/* ********************************************************************************************** */

class FileLogger : public Logger {
 public:
  /**
   * @brief Construct a new FileLogger object
   */
  FileLogger();

  /**
   * @brief Destroy the FileLogger object
   */
  virtual ~FileLogger() = default;

  //! Remove these
  FileLogger(const FileLogger& other) = delete;             // copy constructor
  FileLogger(FileLogger&& other) = delete;                  // move constructor
  FileLogger& operator=(const FileLogger& other) = delete;  // copy assignment
  FileLogger& operator=(FileLogger&& other) = delete;       // move assignment

  //! Override utilities
  void OpenStream() override;
  void CloseStream() override;
  bool IsLoggingEnabled() override;

  void SetFilepath(const std::string& filepath) { filepath_ = filepath; }

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::string filepath_;                               //!< Absolute path for log file
  std::chrono::seconds reopen_interval_;               //!< Interval to reopen file
  std::chrono::system_clock::time_point last_reopen_;  //!< Last timestamp that file was (re)opened
};

/* ********************************************************************************************** */

/**
 * @brief Get unique instance of Logger (singleton pattern)
 * @return Logger instance
 */
inline Logger& get_logger() {
  static std::unique_ptr<Logger> singleton{new FileLogger()};
  return *singleton;
}

/**
 * @brief Get current timestamp in a formatted string
 * @return String containing timestamp
 */
std::string get_timestamp();

}  // namespace util

/* ---------------------------------------------------------------------------------------------- */
/*                                           PUBLIC API                                           */
/* ---------------------------------------------------------------------------------------------- */

//! Parse pre-processing macro to get only filename instead of absolute path from source file
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

//! Macro to log messages (this was the only way found to append "filename:line" in the output)
#define LOG(...) util::get_logger().Log(__FILENAME__, __LINE__, __VA_ARGS__)

//! Macro to log error messages
#define ERROR(...) util::get_logger().Log(__FILENAME__, __LINE__, "ERROR: ", __VA_ARGS__)

#endif  // INCLUDE_UTIL_LOGGER_H_