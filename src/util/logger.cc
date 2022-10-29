#include "util/logger.h"

namespace logger {

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

Logger::Logger(const std::string& filepath)
    : mutex_{},
      filepath_{filepath},
      file_{},
      reopen_interval_{std::chrono::seconds(300)},
      last_reopen_{} {}

/* ********************************************************************************************** */

void Logger::OpenFileStream() {
  auto now = std::chrono::system_clock::now();
  std::scoped_lock<std::mutex> lock{mutex_};

  if ((now - last_reopen_) > reopen_interval_) {
    CloseFileStream();

    try {
      bool init = last_reopen_ == std::chrono::system_clock::time_point();

      // Open file
      file_.open(filepath_, std::ofstream::out | std::ofstream::app);
      last_reopen_ = std::chrono::system_clock::now();

      // In case of first time opening file, write initial message to log
      if (init) {
        std::string header(15, '-');
        file_ << "[" << get_timestamp() << "] " << header << " Initializing log file " << header
              << "\n";
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

void Logger::WriteToFile(const std::string& message) {
  // Check if should (re)open file
  OpenFileStream();

  // Lock mutex and write to file
  std::scoped_lock<std::mutex> lock{mutex_};
  file_ << message;
}

}  // namespace logger