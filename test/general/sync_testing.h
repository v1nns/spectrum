/**
 * \file
 * \brief  Class for synchronized testing
 */

#ifndef INCLUDE_TEST_GENERAL_SYNC_TESTING_H_
#define INCLUDE_TEST_GENERAL_SYNC_TESTING_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

namespace testing {

/**
 * @brief Shared class to syncronize steps between multiple threads used for testing
 */
class TestSyncer {
 public:
  //! Default constructor/destructor
  TestSyncer() = default;
  virtual ~TestSyncer() = default;

  //! Remove these
  TestSyncer(const TestSyncer& other) = delete;             // copy constructor
  TestSyncer(TestSyncer&& other) = delete;                  // move constructor
  TestSyncer& operator=(const TestSyncer& other) = delete;  // copy assignment
  TestSyncer& operator=(TestSyncer&& other) = delete;       // move assignment

  //! Keep blocked until receives desired step (abort test execution if it never comes, otherwise
  //! a broken test would hang forever instead of failing)
  void WaitForStep(int step) {
    auto id = std::this_thread::get_id();
    std::cout << "thread id [" << std::hex << id << std::dec << "] is waiting for step: " << step
              << std::endl;
    std::unique_lock<std::mutex> lock(mutex_);

    if (!cond_var_.wait_for(lock, kStepTimeout, [&] { return step_ == step; })) {
      std::cerr << "thread id [" << std::hex << id << std::dec
                << "] timed out waiting for step: " << step << " (current step: " << step_ << ")"
                << std::endl;
      std::abort();
    }
  }

  //! Notify with new step to unblock the other thread that is waiting for it
  void NotifyStep(int step) {
    auto id = std::this_thread::get_id();
    std::cout << "thread id [" << std::hex << id << std::dec << "] notifying step: " << step
              << std::endl;
    std::unique_lock<std::mutex> lock(mutex_);
    step_ = step;
    cond_var_.notify_all();
  }

 private:
  //! Maximum time to wait for a step (much longer than any step should take)
  static constexpr std::chrono::seconds kStepTimeout{30};

  std::mutex mutex_;
  std::condition_variable cond_var_;
  std::atomic<int> step_{0};  //!< Last notified step (tests notify steps as a ping-pong)
};

//! Default function declaration to run asynchronously
using SyncThread = std::function<void(TestSyncer&)>;

/**
 * @brief Run multiple functions, each one as an unique thread
 * @param functions List of functions informed by test
 */
static inline void RunAsyncTest(std::vector<SyncThread> functions) {
  TestSyncer syncer;
  std::vector<std::thread> threads{};

  for (auto& func : functions) {
    threads.push_back(std::thread(func, std::ref(syncer)));
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

}  // namespace testing
#endif  // INCLUDE_TEST_GENERAL_SYNC_TESTING_H_
