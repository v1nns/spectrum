#include "util/sink.h"

namespace util {

FileSink::FileSink(const std::string& path) : ImplSink<FileSink>(), path_{path} {}

/* ********************************************************************************************** */

void FileSink::Open() {
  auto now = std::chrono::system_clock::now();

  if ((now - last_reopen_) > reopen_interval_) {
    Close();

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