/**
 * \file
 * \brief  Class to run a job in another thread, applying its result later on UI thread
 */

#ifndef INCLUDE_VIEW_ELEMENT_BACKGROUND_JOB_H_
#define INCLUDE_VIEW_ELEMENT_BACKGROUND_JOB_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace interface {

/**
 * @brief Run a job that may take a while (e.g. fetching something from network) in another thread,
 * keeping UI refreshed in the meantime (to animate a spinner). Its result is applied on UI thread,
 * which must call Finish() on every event received (refresh is received as an event)
 */
class BackgroundJob {
  //! Interval to refresh UI while job is running
  static constexpr std::chrono::milliseconds kRefreshInterval{80};

 public:
  //! Function to apply job result (executed on UI thread)
  using Result = std::function<void()>;

  /**
   * @brief Function running in another thread
   * @param cancel Flag set when job must stop as soon as possible
   * @return Function to apply its result (ignored when job was canceled)
   */
  using Task = std::function<Result(const std::atomic<bool>* cancel)>;

  /**
   * @brief Construct a new BackgroundJob object
   * @param refresh Callback to ask for a UI refresh
   * @param name Name of thread running job (for logs and profiling tools)
   */
  BackgroundJob(const std::function<void()>& refresh, const std::string& name);

  /**
   * @brief Destroy BackgroundJob object (canceling job, if running)
   */
  ~BackgroundJob();

  //! Remove these
  BackgroundJob(const BackgroundJob& other) = delete;             // copy constructor
  BackgroundJob(BackgroundJob&& other) = delete;                  // move constructor
  BackgroundJob& operator=(const BackgroundJob& other) = delete;  // copy assignment
  BackgroundJob& operator=(BackgroundJob&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Start job in another thread (canceling the one running, if any)
   * @param task Function to run
   */
  void Start(const Task& task);

  /**
   * @brief Apply job result, if it has finished (must be called on UI thread)
   * @return true if result was applied, otherwise false
   */
  bool Finish();

  /**
   * @brief Cancel job (if running) and discard its result
   */
  void Stop();

  /**
   * @brief Check if job was started and its result was not applied yet
   * @return true if job is running (or waiting for its result to be applied), otherwise false
   */
  bool IsRunning() const { return thread_.joinable(); }

  /**
   * @brief Get time since job started
   * @return Elapsed time
   */
  std::chrono::steady_clock::duration GetElapsed() const {
    return std::chrono::steady_clock::now() - started_;
  }

  /* ******************************************************************************************** */
  //! Internal implementation
 private:
  //! Stop refreshing UI (and wait for its thread)
  void StopRefresh();

  /* ******************************************************************************************** */
  //! Variables

  std::function<void()> refresh_;  //!< Ask for a UI refresh
  std::string name_;               //!< Thread name

  std::thread thread_;                //!< Thread running job
  std::atomic<bool> cancel_ = false;  //!< Flag to cancel job
  std::mutex mutex_;                  //!< Control access to result
  Result result_;                     //!< Result from job, once it finishes

  std::thread refresher_;             //!< Thread refreshing UI while job is running
  std::condition_variable notifier_;  //!< Wake up refresher when job is over
  bool running_ = false;              //!< Flag to keep refresher running (protected by mutex)

  std::chrono::steady_clock::time_point started_;  //!< When job started
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_BACKGROUND_JOB_H_
