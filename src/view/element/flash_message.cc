#include "view/element/flash_message.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "util/logger.h"

namespace interface {

FlashMessage::FlashMessage(Callback on_expire, std::chrono::milliseconds duration)
    : on_expire_{std::move(on_expire)}, duration_{duration} {}

/* ********************************************************************************************** */

FlashMessage::~FlashMessage() {
  {
    const std::scoped_lock lock(mutex_);
    exit_ = true;
  }

  notifier_.notify_one();

  if (thread_.joinable()) {
    thread_.join();
  }
}

/* ********************************************************************************************** */

void FlashMessage::Show(const std::string& text) {
  {
    const std::scoped_lock lock(mutex_);
    text_ = text;
    deadline_ = std::chrono::steady_clock::now() + duration_;

    // Create thread only when it is really needed
    if (!thread_.joinable()) {
      thread_ = std::thread(&FlashMessage::Run, this);
    }
  }

  notifier_.notify_one();
}

/* ********************************************************************************************** */

void FlashMessage::Hide() {
  {
    const std::scoped_lock lock(mutex_);
    text_.reset();
  }

  notifier_.notify_one();
}

/* ********************************************************************************************** */

std::optional<std::string> FlashMessage::GetText() const {
  const std::scoped_lock lock(mutex_);
  return text_;
}

/* ********************************************************************************************** */

void FlashMessage::Run() {
  util::Logger::SetThreadName("message");
  std::unique_lock lock(mutex_);

  while (!exit_) {
    // Nothing to hide, so wait for a new message
    if (!text_.has_value()) {
      notifier_.wait(lock, [this] { return exit_ || text_.has_value(); });
      continue;
    }

    // Wait until message expires (or it is hidden/replaced before that)
    notifier_.wait_until(lock, deadline_, [this] { return exit_ || !text_.has_value(); });

    // Message was replaced by a new one (with a new deadline), hidden or thread must exit
    if (exit_ || !text_.has_value() || std::chrono::steady_clock::now() < deadline_) {
      continue;
    }

    text_.reset();

    // Notify owner without holding the lock
    lock.unlock();
    if (on_expire_) {
      on_expire_();
    }
    lock.lock();
  }
}

}  // namespace interface
