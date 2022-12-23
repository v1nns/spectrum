#include "util/logger.h"

#include <iostream>

namespace util {

std::string get_timestamp() {
  // Get the time
  std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
  std::time_t tt = std::chrono::system_clock::to_time_t(tp);
  std::tm gmt{};
  gmtime_r(&tt, &gmt);
  std::chrono::duration<double> fractional_seconds =
      (tp - std::chrono::system_clock::from_time_t(tt)) + std::chrono::seconds(gmt.tm_sec);

  // Format the string
  std::string buffer("year-mo-dy hr:mn:sc.xxxxxx");
  sprintf(&buffer.front(), "%04d-%02d-%02d %02d:%02d:%09.6f", gmt.tm_year + 1900, gmt.tm_mon + 1,
          gmt.tm_mday, gmt.tm_hour, gmt.tm_min, fractional_seconds.count());

  return buffer;
}

/* ********************************************************************************************** */

void Logger::Configure(const std::string& path) {
  sink_ = std::make_unique<FileSink>(path);

  // Write initial message to log
  std::ostringstream ss;
  std::string header(15, '-');

  ss << header << " Initializing log file " << header << "\n";
  Write(std::move(ss).str());
}

/* ********************************************************************************************** */

void Logger::Configure() { sink_ = std::make_unique<ConsoleSink>(); }

/* ********************************************************************************************** */

void Logger::Write(const std::string& message) {
  // Lock mutex
  std::scoped_lock<std::mutex> lock{mutex_};

  // Check if should (re)open stream
  sink_->OpenStream();

  // Write to output stream
  *sink_ << "[" << get_timestamp() << "] ";
  *sink_ << message;
}

/* ********************************************************************************************** */

FileSink::FileSink(const std::string& path)
    : Sink(), path_{path}, reopen_interval_{std::chrono::seconds(300)}, last_reopen_{} {}

/* ********************************************************************************************** */

void FileSink::OpenStream() {
  auto now = std::chrono::system_clock::now();

  if ((now - last_reopen_) > reopen_interval_) {
    CloseStream();

    try {
      // Open file
      out_stream_.reset(new std::ofstream(path_, std::ofstream::out | std::ofstream::app));
      last_reopen_ = now;
    } catch (std::exception& e) {
      CloseStream();
      throw e;
    }
  }
}

/* ********************************************************************************************** */

void FileSink::CloseStream() {
  try {
    out_stream_.reset();
  } catch (...) {
  }
}

/* ********************************************************************************************** */

void ConsoleSink::OpenStream() {
  if (!out_stream_) {
    // No-operation deleter, otherwise we will get in trouble
    out_stream_.reset(&std::cout, [](void*) {});
  }
}

/* ********************************************************************************************** */

void ConsoleSink::CloseStream() {
  try {
    out_stream_.reset();
  } catch (...) {
  }
}

}  // namespace util