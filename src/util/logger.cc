#include "util/logger.h"

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

Logger::Logger()
    : mutex_{}, filepath_{}, file_{}, reopen_interval_{std::chrono::seconds(300)}, last_reopen_{} {}

/* ********************************************************************************************** */

void Logger::OpenFileStream(const std::string& timestamp) {
  auto now = std::chrono::system_clock::now();

  if ((now - last_reopen_) > reopen_interval_) {
    CloseFileStream();

    try {
      bool first_message = last_reopen_ == std::chrono::system_clock::time_point();

      // Open file
      file_.open(filepath_, std::ofstream::out | std::ofstream::app);
      last_reopen_ = now;

      // In case of first time opening file, write initial message to log
      if (first_message) {
        std::string header(15, '-');
        file_ << "[" << timestamp << "] " << header << " Initializing log file " << header << "\n";
      }
    } catch (std::exception& e) {
      CloseFileStream();
      throw e;
    }
  }
}

/* ********************************************************************************************** */

void Logger::CloseFileStream() {
  try {
    file_.close();
  } catch (...) {
  }
}

/* ********************************************************************************************** */

void Logger::WriteToFile(const std::string& timestamp, const std::string& message) {
  // Lock mutex
  std::scoped_lock<std::mutex> lock{mutex_};

  // Check if should (re)open file
  OpenFileStream(timestamp);

  // Write to file
  file_ << message;
}

}  // namespace util