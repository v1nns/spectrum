#include "view/element/background_job.h"

#include <utility>

#include "util/logger.h"

namespace interface {

BackgroundJob::BackgroundJob(const std::function<void()>& refresh, const std::string& name)
    : refresh_{refresh}, name_{name} {}

/* ********************************************************************************************** */

BackgroundJob::~BackgroundJob() { Stop(); }

/* ********************************************************************************************** */

void BackgroundJob::Start(const Task& task) {
  Stop();

  cancel_ = false;
  running_ = true;
  started_ = std::chrono::steady_clock::now();

  // Spinner is animated while job is running, so UI must be refreshed in the meantime
  refresher_ = std::thread([this] {
    util::Logger::SetThreadName(name_);
    std::unique_lock lock{mutex_};

    while (!notifier_.wait_for(lock, kRefreshInterval, [this] { return !running_; })) {
      lock.unlock();
      refresh_();
      lock.lock();
    }
  });

  thread_ = std::thread([this, task] {
    util::Logger::SetThreadName(name_);

    Result result = task(&cancel_);
    if (cancel_) return;

    {
      std::scoped_lock lock{mutex_};
      result_ = result ? std::move(result) : [] {};
    }

    // Ask for a refresh, so result is applied right away
    refresh_();
  });
}

/* ********************************************************************************************** */

bool BackgroundJob::Finish() {
  Result result;

  {
    std::scoped_lock lock{mutex_};
    if (!result_) return false;
    result.swap(result_);
  }

  // Thread has already finished its job
  if (thread_.joinable()) thread_.join();
  StopRefresh();

  result();
  return true;
}

/* ********************************************************************************************** */

void BackgroundJob::Stop() {
  if (!thread_.joinable()) return;

  LOG("Stop job running in thread=", name_);
  cancel_ = true;
  thread_.join();
  StopRefresh();

  std::scoped_lock lock{mutex_};
  result_ = nullptr;
}

/* ********************************************************************************************** */

void BackgroundJob::StopRefresh() {
  if (!refresher_.joinable()) return;

  {
    std::scoped_lock lock{mutex_};
    running_ = false;
  }

  notifier_.notify_one();
  refresher_.join();
}

}  // namespace interface
