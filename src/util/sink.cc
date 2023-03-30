#include "util/sink.h"

namespace util {

FileSink::FileSink(const std::string& path)
    : ImplSink<FileSink>(),
      path_{path},
      reopen_interval_{std::chrono::seconds(300)},
      last_reopen_{} {}

/* ********************************************************************************************** */

void FileSink::Open() {
  auto now = std::chrono::system_clock::now();

  if ((now - last_reopen_) > reopen_interval_) {
    Close();

    try {
      // Open file
      out_stream_.reset(new std::ofstream(path_, std::ofstream::out | std::ofstream::app));
      last_reopen_ = now;
    } catch (std::exception& e) {
      CloseStream();
      throw;
    }
  }
}

/* ********************************************************************************************** */

void FileSink::Close() {
  try {
    out_stream_.reset();
  } catch (...) {
  }
}

/* ********************************************************************************************** */

void ConsoleSink::Open() {
  if (!out_stream_) {
    // No-operation deleter, otherwise we will get in trouble
    out_stream_.reset(&std::cout, [](void*) {});
  }
}

/* ********************************************************************************************** */

void ConsoleSink::Close() {
  try {
    out_stream_.reset();
  } catch (...) {
  }
}

}  // namespace util