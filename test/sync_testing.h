/**
 * \file
 * \brief  Class for synchronized testing
 */

#ifndef INCLUDE_TEST_SYNC_TESTING_H_
#define INCLUDE_TEST_SYNC_TESTING_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

namespace testing {

/**
 * @brief Shared class to manage threads from testing
 */
class SyncTesting {
 public:
  //! Default constructor/destructor
  SyncTesting() = default;
  virtual ~SyncTesting() = default;

  //! Keep blocked until receives desired step
  void WaitForStep(int step) {
    auto id = std::this_thread::get_id();
    std::cout << "thread id [" << id << "] is waiting for step: " << step << std::endl;
    std::unique_lock<std::mutex> lock(mutex_);
    cond_var_.wait(lock, [&] { return step_ == step; });
  }

  //! Notify with new step to unblock the other thread that is waiting for it
  void NotifyStep(int step) {
    auto id = std::this_thread::get_id();
    std::cout << "thread id [" << id << "] notifying step: " << step << std::endl;
    std::unique_lock<std::mutex> lock(mutex_);
    step_ = step;
    cond_var_.notify_one();
  }

  //! Remove these
  SyncTesting(const SyncTesting& other) = delete;             // copy constructor
  SyncTesting(SyncTesting&& other) = delete;                  // move constructor
  SyncTesting& operator=(const SyncTesting& other) = delete;  // copy assignment
  SyncTesting& operator=(SyncTesting&& other) = delete;       // move assignment

 private:
  std::mutex mutex_;
  std::condition_variable cond_var_;
  std::atomic<int> step_;  // TODO: change for "queue"
};

using SyncThread = std::function<void(SyncTesting&)>;

/**
 * @brief Run multiple functions, each one as an unique thread
 * @param functions List of functions informed by test
 */
static inline void RunAsyncTest(std::vector<SyncThread> functions) {
  SyncTesting sync;
  std::vector<std::thread> threads{};

  for (auto& func : functions) {
    threads.push_back(std::thread(func, std::ref(sync)));
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

}  // namespace testing
#endif  // INCLUDE_TEST_SYNC_TESTING_H_