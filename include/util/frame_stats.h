/**
 * \file
 * \brief  Class for measuring how often a frame is produced and how long it takes
 */

#ifndef INCLUDE_UTIL_FRAME_STATS_H_
#define INCLUDE_UTIL_FRAME_STATS_H_

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

#include "util/logger.h"

namespace util {

/**
 * @brief Count frames and their duration, writing a summary to log once per second (only logged
 * with verbose logging), e.g. "analysis fps=60 avg=85us max=140us gap=18ms" (gap is the longest
 * time between two frames, which shows how even they are spread)
 */
class FrameStats {
  using Clock = std::chrono::steady_clock;
  static constexpr std::chrono::seconds kReportInterval{1};

 public:
  /**
   * @brief Create a new FrameStats
   * @param name Name shown in log messages
   */
  explicit FrameStats(std::string name) : name_{std::move(name)} {}

  /**
   * @brief Register a new frame
   * @param duration Time spent to produce frame
   */
  void Tick(Clock::duration duration) {
    const auto now = Clock::now();

    frames_++;
    total_ += duration;
    max_ = std::max(max_, duration);
    gap_ = std::max(gap_, now - last_);
    last_ = now;

    if (now - start_ < kReportInterval) return;

    using std::chrono::duration_cast;
    using std::chrono::microseconds;
    using std::chrono::milliseconds;

    const double elapsed = std::chrono::duration<double>(now - start_).count();

    LOG(name_, " fps=", static_cast<int>(frames_ / elapsed),
        " avg=", duration_cast<microseconds>(total_ / frames_).count(),
        "us max=", duration_cast<microseconds>(max_).count(),
        "us gap=", duration_cast<milliseconds>(gap_).count(), "ms");

    start_ = now;
    frames_ = 0;
    total_ = Clock::duration::zero();
    max_ = Clock::duration::zero();
    gap_ = Clock::duration::zero();
  }

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::string name_;                                 //!< Name shown in log messages
  Clock::time_point start_ = Clock::now();           //!< Start of current report interval
  int frames_ = 0;                                   //!< Frames in current report interval
  Clock::duration total_ = Clock::duration::zero();  //!< Sum of frame durations
  Clock::duration max_ = Clock::duration::zero();    //!< Longest frame duration
  Clock::time_point last_ = Clock::now();            //!< When last frame was registered
  Clock::duration gap_ = Clock::duration::zero();    //!< Longest time between two frames
};

}  // namespace util
#endif  // INCLUDE_UTIL_FRAME_STATS_H_
