/**
 * \file
 * \brief Header for Text Animation element
 */

#ifndef INCLUDE_VIEW_ELEMENT_TEXT_ANIMATION_H_
#define INCLUDE_VIEW_ELEMENT_TEXT_ANIMATION_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace interface {

/**
 * @brief An structure to offset selected entry text when its content is too long
 */
struct TextAnimation {
  using Callback = std::function<void()>;  //!< Callback triggered by internal thread

  std::mutex mutex;                  //!< Control access for internal resources
  std::condition_variable notifier;  //!< Conditional variable to block thread
  std::thread thread;                //!< Thread to perform offset animation on text

  std::atomic<bool> enabled = false;  //!< Flag to control thread animation
  std::string text;                   //!< Entry text to perform animation

  Callback cb_update;  //!< Force an UI refresh

  //! Destructor
  ~TextAnimation();

  /**
   * @brief Start animation thread
   * @param entry Text content from selected entry
   */
  void Start(const std::string& entry);

  /**
   * @brief Stop animation thread
   */
  void Stop();

 private:
  /**
   * @brief Disable thread execution
   */
  void Notify();

  /**
   * @brief Notify thread and wait for its stop
   */
  void Exit();
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_TEXT_ANIMATION_H_
