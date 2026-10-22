#include "util/loop_waker.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <tuple>

namespace util {

LoopWaker::LoopWaker()
    : event_fd_{eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)},
      tty_fd_{open("/dev/tty", O_RDONLY | O_NONBLOCK | O_CLOEXEC)} {}

/* ********************************************************************************************** */

LoopWaker::~LoopWaker() {
  if (event_fd_ >= 0) close(event_fd_);
  if (tty_fd_ >= 0) close(tty_fd_);
}

/* ********************************************************************************************** */

void LoopWaker::Notify() const {
  const uint64_t value = 1;
  if (event_fd_ >= 0) std::ignore = write(event_fd_, &value, sizeof(value));
}

/* ********************************************************************************************** */

void LoopWaker::Wait() const {
  // Terminal input is only checked here (FTXUI is the one to read it)
  std::array<pollfd, 2> fds{{{event_fd_, POLLIN, 0}, {tty_fd_, POLLIN, 0}}};

  if (poll(fds.data(), fds.size(), kMaxWaitMs) > 0 && (fds[0].revents & POLLIN)) {
    uint64_t value = 0;
    std::ignore = read(event_fd_, &value, sizeof(value));
  }
}

}  // namespace util
