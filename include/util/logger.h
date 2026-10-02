/**
 * \file
 * \brief  Class for logging with the possibility to choose which output stream to use
 */

#ifndef INCLUDE_UTIL_LOGGER_H_
#define INCLUDE_UTIL_LOGGER_H_

#include <cxxabi.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "util/sink.h"

namespace util {

//! Log level, from the most detailed to the most severe
enum class LogLevel : std::uint8_t {
  Debug,    //!< Detailed steps, only useful when debugging (enabled with verbose logging)
  Info,     //!< Meaningful actions and state changes
  Warning,  //!< Unexpected situation that does not stop the application from working
  Error,    //!< Operation that failed
};

/**
 * @brief Responsible for message logging (thread-safe) to a defined output stream
 */
class Logger {
  static constexpr int kHeaderColumns = 41;  //!< Number of columns to write on log initialization

 protected:
  /**
   * @brief Construct a new Logger object
   */
  Logger() = default;

 public:
  /**
   * @brief Destroy the Logger object
   */
  ~Logger() = default;

  //! Remove these
  Logger(const Logger& other) = delete;             // copy constructor
  Logger(Logger&& other) = delete;                  // move constructor
  Logger& operator=(const Logger& other) = delete;  // copy assignment
  Logger& operator=(Logger&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Public API
  /**
   * @brief Get unique instance of Logger
   * @return Logger instance
   */
  static Logger& GetInstance() {
    // Simply extend the Logger class, as we do not want to expose the default constructor,
    // neither do we want to use std::make_unique explicitly calling operator new()
    struct MakeUniqueEnabler : public Logger {
      using Logger::Logger;
    };
    static std::unique_ptr<Logger> singleton = std::make_unique<MakeUniqueEnabler>();
    return *singleton;
  }

  /**
   * @brief Enable logging with customized settings
   * @param path Log filepath
   */
  void Configure(const std::string& path);

  /**
   * @brief Enable logging to stdout
   */
  void Configure();

  /**
   * @brief Set minimum level of messages to log (by default, debug messages are not logged)
   * @param level Minimum log level
   */
  void SetLevel(LogLevel level) { level_ = level; }

  //! Check if messages with the given level are logged
  bool IsEnabled(LogLevel level) const { return sink_ && level >= level_; }

  /**
   * @brief Concatenate all arguments into a single string and write it to output stream
   * @tparam ...Args Splitted arguments
   * @param level Message level
   * @param filename Current file name
   * @param line Current line number
   * @param ...args Arguments to build log message
   */
  template <typename... Args>
  inline void Log(LogLevel level, const char* filename, int line, Args&&... args) {
    // Do nothing if sink is not configured or level is not enabled (before building message)
    if (!IsEnabled(level)) return;

    // Build log message and write it to output stream
    std::ostringstream ss;

    ss << "[" << std::hex << std::this_thread::get_id() << std::dec << "] ";
    ss << "[" << GetLevelName(level) << "] ";
    ss << "[" << filename << ":" << line << "] ";
    (ss << ... << std::forward<Args>(args)) << "\n";

    Write(std::move(ss).str());
  }

  /* ******************************************************************************************** */
  //! Utility
 private:
  /**
   * @brief Write message to output stream buffer
   * @param message String message
   * @param add_timestamp Control flag to insert a timestamp as preffix
   */
  void Write(const std::string& message, bool add_timestamp = true);

  //! Get level name (with fixed width, to keep log columns aligned)
  static const char* GetLevelName(LogLevel level) {
    switch (level) {
      case LogLevel::Debug:
        return "DEBUG";
      case LogLevel::Info:
        return "INFO ";
      case LogLevel::Warning:
        return "WARN ";
      case LogLevel::Error:
        break;
    }

    return "ERROR";
  }

  /* ******************************************************************************************** */
  //! Variables

  std::mutex mutex_;                              //!< Control access for internal resources
  std::unique_ptr<Sink> sink_;                    //!< Sink to stream output message
  std::atomic<LogLevel> level_ = LogLevel::Info;  //!< Minimum level of messages to log
};

/* ********************************************************************************************** */

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

//! Macro to log messages with the given level (the only way found to append "filename:line")
#define LOG_LEVEL(level, ...) \
  util::Logger::GetInstance().Log(level, __FILENAME__, __LINE__, __VA_ARGS__)

//! Macro to log detailed steps (only logged with verbose logging)
#define LOG(...) LOG_LEVEL(util::LogLevel::Debug, __VA_ARGS__)

//! Macro to log detailed steps based on condition
#define LOG_IF(condition, ...) \
  if (condition) LOG(__VA_ARGS__)

//! Macro to log meaningful actions and state changes
#define INFO(...) LOG_LEVEL(util::LogLevel::Info, __VA_ARGS__)

//! Macro to log unexpected situations that do not stop the application from working
#define WARN(...) LOG_LEVEL(util::LogLevel::Warning, __VA_ARGS__)

//! Macro to log error messages
#define ERROR(...) LOG_LEVEL(util::LogLevel::Error, __VA_ARGS__)

//! Macro to log error messages based on condition
#define ERROR_IF(condition, ...) \
  if (condition) ERROR(__VA_ARGS__)

/* ---------------------------------------------------------------------------------------------- */
/*                                        TEMPLATE FRIENDLY                                       */
/* ---------------------------------------------------------------------------------------------- */

//! Return a human-readable string containing the implementation-defined name of the type
inline std::string demangle(const char* name) {
  // Some arbitrary value to eliminate the compiler warning
  int status = -99;

  std::unique_ptr<char, void (*)(void*)> res{abi::__cxa_demangle(name, nullptr, nullptr, &status),
                                             [](void* a) { return std::free(a); }};

  std::ostringstream ss;
  ss << "[" << (status == 0 ? res.get() : name) << "] ";

  return std::move(ss).str();
}

//! Macro to log messages including the class type in a human-readable format
#define LOG_T(...) LOG(demangle(typeid(*this).name()), __VA_ARGS__)

//! Macro to log messages based on condition and including the class type in a human-readable format
#define LOG_T_IF(condition, ...) LOG_IF(condition, demangle(typeid(*this).name()), __VA_ARGS__)

//! Macro to log error messages including the class type in a human-readable format
#define ERROR_T(...) ERROR(demangle(typeid(*this).name()), __VA_ARGS__)

//! Macro to log errors based on condition and including the class type in a human-readable format
#define ERROR_T_IF(condition, ...) ERROR_IF(condition, demangle(typeid(*this).name()), __VA_ARGS__)

#endif  // INCLUDE_UTIL_LOGGER_H_
