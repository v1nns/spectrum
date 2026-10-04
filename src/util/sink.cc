#include "util/sink.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace util {

FileSink::FileSink(const std::string& path, std::uintmax_t max_size)
    : ImplSink<FileSink>(), path_{path}, max_size_{max_size} {}

/* ********************************************************************************************** */

void FileSink::Open() {
  auto now = std::chrono::system_clock::now();

  if ((now - last_reopen_) > reopen_interval_) {
    Close();
    RotateIfNeeded();

    try {
      // Open file
      out_stream_.reset(new std::ofstream(path_, std::ofstream::out | std::ofstream::app));
      last_reopen_ = now;
    } catch (std::exception&) {
      CloseStream();
      throw;
    }
  }
}

/* ********************************************************************************************** */

void FileSink::RotateIfNeeded() const {
  std::error_code error;
  if (auto size = std::filesystem::file_size(path_, error); error || size < max_size_) return;

  // Keep only the previous log file (logger may not be available to report any error here)
  std::filesystem::rename(path_, path_ + ".1", error);
}

/* ********************************************************************************************** */

void FileSink::Close() {
  try {
    out_stream_.reset();
  } catch (...) {
    // As we hold an ostream inside a shared_ptr, when we reset it, we are counting on the deleter
    // to release its resources... And that's why we don't care about exceptions here
  }
}

/* ********************************************************************************************** */

void ConsoleSink::Open() {
  if (!out_stream_) {
    // No-operation deleter, otherwise we will get in trouble
    out_stream_.reset(&std::cout, [](const void*) {
      // For this sink, it is used the std::cout itself, so when the shared_ptr is reset, no deleter
      // should be called for the object
    });
  }
}

/* ********************************************************************************************** */

void ConsoleSink::Close() {
  try {
    out_stream_.reset();
  } catch (...) {
    // As we hold an ostream inside a shared_ptr, when we reset it, we are counting on the deleter
    // to release its resources... And that's why we don't care about exceptions here
  }
}

}  // namespace util
