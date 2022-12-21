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

Logger& Logger::Configure(const std::string& filepath) {
  auto& logger = reinterpret_cast<FileLogger&>(get_logger());
  logger.SetFilepath(filepath);
  return logger;
}

/* ********************************************************************************************** */

Logger::Logger() : mutex_{}, out_stream_{nullptr} {}

/* ********************************************************************************************** */

Logger::~Logger() { CloseStream(); }

/* ********************************************************************************************** */

void Logger::OpenStream() {}

/* ********************************************************************************************** */

void Logger::CloseStream() {}

/* ********************************************************************************************** */

bool Logger::IsLoggingEnabled() { return false; }

/* ********************************************************************************************** */

void Logger::Write(const std::string& message) {
  // Lock mutex
  std::scoped_lock<std::mutex> lock{mutex_};

  // Check if should (re)open stream
  OpenStream();

  if (out_stream_) {
    // Write to output stream
    *out_stream_ << "[" << get_timestamp() << "] ";
    *out_stream_ << message << std::flush;
  }
}

/* ********************************************************************************************** */

FileLogger::FileLogger()
    : filepath_{}, reopen_interval_{std::chrono::seconds(300)}, last_reopen_{} {}

/* ********************************************************************************************** */

void FileLogger::OpenStream() {
  auto now = std::chrono::system_clock::now();

  if ((now - last_reopen_) > reopen_interval_) {
    CloseStream();

    try {
      bool first_message = last_reopen_ == std::chrono::system_clock::time_point();

      // Open file
      out_stream_.reset(new std::ofstream(filepath_, std::ofstream::out | std::ofstream::app));
      last_reopen_ = now;

      // In case of first time opening file, write initial message to log
      if (first_message) {
        std::string header(15, '-');
        *out_stream_ << "[" << get_timestamp() << "] ";
        *out_stream_ << header << " Initializing log file " << header << "\n";
      }
    } catch (std::exception& e) {
      CloseStream();
      throw e;
    }
  }
}

/* ********************************************************************************************** */

void FileLogger::CloseStream() {
  try {
    out_stream_.reset();
  } catch (...) {
  }
}

/* ********************************************************************************************** */

bool FileLogger::IsLoggingEnabled() {
  // Logging is optional, so if no path has been provided, do nothing
  return !filepath_.empty();
}

}  // namespace util