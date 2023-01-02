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
 * @brief Responsible for controlling the output streambuffer where log messages will be written
 */
class Sink {
 protected:
  //! Construct a new Sink object (only done by derived classes)
  Sink() = default;

 public:
  //! Destroy the Sink object
  virtual ~Sink() { CloseStream(); };

  //! Remove these
  Sink(const Sink& other) = delete;             // copy constructor
  Sink(Sink&& other) = delete;                  // move constructor
  Sink& operator=(const Sink& other) = delete;  // copy assignment
  Sink& operator=(Sink&& other) = delete;       // move assignment

  /**
   * @brief Forward argument to inner output stream object
   * @tparam T Argument typename definition
   * @param t Argument
   * @return Sink object
   */
  template <typename T>
  Sink& operator<<(T&& t) {
    if (out_stream_) {
      *out_stream_ << std::forward<T>(t);
      out_stream_->flush();  // In order to ensure that message will be written even if application
                             // crashes, force a flush to output stream buffer
    }
    return *this;
  }

  /* ******************************************************************************************** */
  //! Override these methods on derived classes

  virtual void OpenStream() {}
  virtual void CloseStream() {}

  /* ******************************************************************************************** */
  //! Variables
 protected:
  std::shared_ptr<std::ostream> out_stream_;  //!< Output stream buffer to write messages
};

/* ********************************************************************************************** */

/**
 * @brief Responsible for message logging (thread-safe) to a defined output stream
 */
class Logger {
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
 public:
  /**
   * @brief Get unique instance of Logger
   * @return Logger instance
   */
  static Logger& GetInstance() {
    static std::unique_ptr<Logger> singleton{new Logger()};
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
  void Log(const char* filename, int line, Args&&... args) {
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
   */
  void Write(const std::string& message);

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::mutex mutex_;            //!< Control access for internal resources
  std::unique_ptr<Sink> sink_;  //!< Sink to stream output message
};

/* ---------------------------------------------------------------------------------------------- */
/*                                           FILE LOGGER                                          */
/* ---------------------------------------------------------------------------------------------- */

class FileSink : public Sink {
 public:
  explicit FileSink(const std::string& path);
  virtual ~FileSink(){};

  //! Overriden methods
  void OpenStream() override;
  void CloseStream() override;

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::string path_;                                   //!< Absolute path for log file
  std::chrono::seconds reopen_interval_;               //!< Interval to reopen file
  std::chrono::system_clock::time_point last_reopen_;  //!< Last timestamp that file was (re)opened
};

/* ---------------------------------------------------------------------------------------------- */
/*                                          STDOUT LOGGER                                         */
/* ---------------------------------------------------------------------------------------------- */

class ConsoleSink : public Sink {
 public:
  using Sink::Sink;

  //! Overriden methods
  void OpenStream() override;
  void CloseStream() override;
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

//! Macro to log error messages
#define ERROR(...) util::Logger::GetInstance().Log(__FILENAME__, __LINE__, "ERROR: ", __VA_ARGS__)

#endif  // INCLUDE_UTIL_LOGGER_H_