/**
 * \file
 * \brief  Class for logging with the possibility to choose which output stream to use
 */

#ifndef INCLUDE_UTIL_LOGGER_H_
#define INCLUDE_UTIL_LOGGER_H_

#include <cxxabi.h>

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
   * @brief Concatenate all arguments into a single string and write it to output stream
   * @tparam ...Args Splitted arguments
   * @param filename Current file name
   * @param line Current line number
   * @param ...args Arguments to build log message
   */
  template <typename... Args>
  inline void Log(const char* filename, int line, Args&&... args) {
    // Do nothing if sink is not configured
    if (!sink_) return;

    // Build log message and write it to output stream
    std::ostringstream ss;

    ss << "[" << std::hex << std::this_thread::get_id() << std::dec << "] ";
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

  /* ******************************************************************************************** */
  //! Variables

  std::mutex mutex_;            //!< Control access for internal resources
  std::unique_ptr<Sink> sink_;  //!< Sink to stream output message
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

//! Macro to log messages (this was the only way found to append "filename:line" in the output)
#define LOG(...) util::Logger::GetInstance().Log(__FILENAME__, __LINE__, __VA_ARGS__)

//! Macro to log messages based on condition
#define LOG_IF(condition, ...) \
  if (condition) util::Logger::GetInstance().Log(__FILENAME__, __LINE__, __VA_ARGS__)

//! Macro to log error messages
#define ERROR(...) util::Logger::GetInstance().Log(__FILENAME__, __LINE__, "ERROR: ", __VA_ARGS__)

//! Macro to log error messages based on condition
#define ERROR_IF(condition, ...) \
  if (condition) util::Logger::GetInstance().Log(__FILENAME__, __LINE__, "ERROR: ", __VA_ARGS__)

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
