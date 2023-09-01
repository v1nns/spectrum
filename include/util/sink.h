/**
 * \file
 * \brief  Class that represents an output for logging
 */

#ifndef INCLUDE_UTIL_SINK_H_
#define INCLUDE_UTIL_SINK_H_

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace util {

/**
 * @brief Interface to manage Sink
 */
class Sink {
 public:
  virtual ~Sink() = default;

  /* ******************************************************************************************** */
  //! Public API

  virtual void OpenStream() = 0;
  virtual void CloseStream() = 0;

  /**
   * @brief Forward argument to inner output stream object
   * @tparam T Argument typename definition
   * @param t Argument
   * @return Sink object
   */
  template <typename Arg>
  Sink& operator<<(Arg&& t) {
    WriteToStream(std::forward<Arg>(t));
    return *this;
  }

  /* ******************************************************************************************** */
  //! Internal methods
 protected:
  virtual void WriteToStream([[maybe_unused]] const std::string& message){
      // This method may not be used if there isn't a valid ostream opened
  };
};

/* ********************************************************************************************** */

/**
 * @brief Responsible for controlling the output streambuffer where log messages will be written
 * P.S. using CRTP pattern
 */
template <typename T>
class ImplSink : public Sink {
 protected:
  //! Construct a new ImplSink object (only done by derived classes)
  ImplSink() = default;

 public:
  //! Destroy the ImplSink object
  ~ImplSink() override { CloseStream(); };

  //! Remove these
  ImplSink(const ImplSink& other) = delete;             // copy constructor
  ImplSink(ImplSink&& other) = delete;                  // move constructor
  ImplSink& operator=(const ImplSink& other) = delete;  // copy assignment
  ImplSink& operator=(ImplSink&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Overridden methods

  //! Open output stream
  void OpenStream() final { static_cast<T&>(*this).Open(); }

  //! Close output stream
  void CloseStream() final { static_cast<T&>(*this).Close(); }

 private:
  //! Write message to output stream
  void WriteToStream(const std::string& message) final {
    if (out_stream_) {
      *out_stream_ << message;
      out_stream_->flush();  // In order to ensure that message will be written even if application
                             // crashes, force a flush to output stream buffer
    }
  }

  /* ******************************************************************************************** */
  //! Variables
 protected:
  std::shared_ptr<std::ostream> out_stream_;  //!< Output stream buffer to write messages
};

/* ---------------------------------------------------------------------------------------------- */
/*                                           FILE LOGGER                                          */
/* ---------------------------------------------------------------------------------------------- */

class FileSink : public ImplSink<FileSink> {
 public:
  explicit FileSink(const std::string& path);
  ~FileSink() override = default;

  //!  Required methods
  void Open();
  void Close();

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::string path_;  //!< Absolute path for log file
  std::chrono::seconds reopen_interval_ = std::chrono::seconds(300);  //!< Interval to reopen file
  std::chrono::system_clock::time_point last_reopen_;  //!< Last timestamp that file was (re)opened
};

/* ---------------------------------------------------------------------------------------------- */
/*                                          STDOUT LOGGER                                         */
/* ---------------------------------------------------------------------------------------------- */

class ConsoleSink : public ImplSink<ConsoleSink> {
 public:
  using ImplSink<ConsoleSink>::ImplSink;

  //!  Required methods
  void Open();
  void Close();
};

}  // namespace util

#endif  // INCLUDE_UTIL_SINK_H_
