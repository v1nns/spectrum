#include "util/logger.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace util {

std::string get_timestamp() {
  // Get time
  std::chrono::system_clock::time_point time_point = std::chrono::system_clock::now();
  std::time_t now = std::chrono::system_clock::to_time_t(time_point);

  // Convert into local time
  struct tm local_time;
  localtime_r(&now, &local_time);

  // Get the fractional part in milliseconds
  const auto milliseconds = std::chrono::time_point_cast<std::chrono::milliseconds>(time_point)
                                .time_since_epoch()
                                .count() %
                            1000;

  // Format string
  std::ostringstream ss;
  ss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3)
     << milliseconds;

  return std::move(ss).str();
}

/* ********************************************************************************************** */

void Logger::Configure(const std::string& path) {
  sink_ = std::make_unique<FileSink>(path);

  // Write initial message to log
  std::ostringstream ss;
  std::string header(kHeaderColumns, '-');

  ss << header << " Initializing log " << header << "\n";
  Write(std::move(ss).str(), false);
}

/* ********************************************************************************************** */

void Logger::Configure() { sink_ = std::make_unique<ConsoleSink>(); }

/* ********************************************************************************************** */

void Logger::Write(const std::string& message, bool add_timestamp) {
  // Lock mutex
  std::scoped_lock<std::mutex> lock{mutex_};

  // Check if should (re)open stream
  sink_->OpenStream();

  // Write to output stream
  if (add_timestamp) *sink_ << "[" << get_timestamp() << "] ";
  *sink_ << message;
}

}  // namespace util
