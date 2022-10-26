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
#include <string>

namespace logger {

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
   * @brief Write log message to file
   * @param filename Current file name
   * @param line Current line number
   * @param message Log message
   */
  void Log(const char* filename, int line, const std::string& message);

 private:
  std::mutex mutex_;                                   //!< Control access for internal resources
  std::string filepath_;                               //!< Absolute path for log file
  std::ofstream file_;                                 //!< Output stream to operate on log file
  std::chrono::seconds reopen_interval_;               //!< Interval to reopen file
  std::chrono::system_clock::time_point last_reopen_;  //!< Last timestamp that file was (re)opened
};

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

}  // namespace logger

/* ---------------------------------------------------------------------------------------------- */
/*                                           PUBLIC API                                           */
/* ---------------------------------------------------------------------------------------------- */

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG(message) logger::get_logger().Log(__FILENAME__, __LINE__, message)

#endif  // INCLUDE_UTIL_LOGGER_H_