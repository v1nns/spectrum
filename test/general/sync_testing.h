/**
 * \file
 * \brief  Class for synchronized testing
 */

#ifndef INCLUDE_TEST_GENERAL_SYNC_TESTING_H_
#define INCLUDE_TEST_GENERAL_SYNC_TESTING_H_

#include <atomic>
#include <condition_variable>
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

 private:
  std::mutex mutex_;
  std::condition_variable cond_var_;
  std::atomic<int> step_;  // TODO: change for "queue"
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
