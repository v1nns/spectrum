/**
 * \file
 * \brief  Wake up GUI loop as soon as there is something to handle
 */

#ifndef INCLUDE_UTIL_LOOP_WAKER_H_
#define INCLUDE_UTIL_LOOP_WAKER_H_

namespace util {

/**
 * @brief Wakes up GUI loop as soon as there is something to handle: an event sent by spectrum or
 * input from terminal. FTXUI loop only checks for them once every 16 ms, so frames sent by audio
 * visualizer at the same rate would be delayed and sometimes merged (dropping frames)
 */
class LoopWaker {
 public:
  //! Maximum time to wait, so FTXUI keeps doing anything else on time (e.g. tell Escape key apart
  //! from the beginning of a key sequence, which takes 50 ms without any other input)
  static constexpr int kMaxWaitMs = 50;

  /**
   * @brief Construct a new LoopWaker object
   */
  LoopWaker();

  /**
   * @brief Destroy the LoopWaker object
   */
  ~LoopWaker();

  //! Remove these
  LoopWaker(const LoopWaker& other) = delete;             // copy constructor
  LoopWaker(LoopWaker&& other) = delete;                  // move constructor
  LoopWaker& operator=(const LoopWaker& other) = delete;  // copy assignment
  LoopWaker& operator=(LoopWaker&& other) = delete;       // move assignment

  /**
   * @brief Wake up loop (thread-safe)
   */
  void Notify() const;

  /**
   * @brief Block until an event is sent, terminal has input to read or maximum time has passed
   */
  void Wait() const;

 private:
  int event_fd_;  //!< Signaled when spectrum sends an event
  int tty_fd_;    //!< Terminal, to know when it has input
};

}  // namespace util
#endif  // INCLUDE_UTIL_LOOP_WAKER_H_
