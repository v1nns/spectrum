/**
 * \file
 * \brief Header for Flash Message element
 */

#ifndef INCLUDE_VIEW_ELEMENT_FLASH_MESSAGE_H_
#define INCLUDE_VIEW_ELEMENT_FLASH_MESSAGE_H_

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace interface {

/**
 * @brief Keep a short text visible only for a limited time (e.g. to give feedback to user). When
 * the text expires, the internal thread notifies the owner to refresh the UI
 */
class FlashMessage final {
 public:
  using Callback = std::function<void()>;  //!< Callback triggered when message expires

  /**
   * @brief Construct a new FlashMessage object
   * @param on_expire Callback to notify that message is no longer visible (to refresh UI)
   * @param duration Time that message stays visible
   */
  FlashMessage(Callback on_expire, std::chrono::milliseconds duration);

  /**
   * @brief Destroy the FlashMessage object (without triggering the expiration callback)
   */
  ~FlashMessage();

  //! Remove these
  FlashMessage(const FlashMessage& other) = delete;             // copy constructor
  FlashMessage(FlashMessage&& other) = delete;                  // move constructor
  FlashMessage& operator=(const FlashMessage& other) = delete;  // copy assignment
  FlashMessage& operator=(FlashMessage&& other) = delete;       // move assignment

  /**
   * @brief Show a new message (replacing the current one and restarting the timer)
   * @param text Message content
   */
  void Show(const std::string& text);

  /**
   * @brief Hide current message immediately (expiration callback is not triggered)
   */
  void Hide();

  /**
   * @brief Get message content, if still visible
   * @return Message content, otherwise empty
   */
  [[nodiscard]] std::optional<std::string> GetText() const;

 private:
  /**
   * @brief Thread loop that clears message once it expires
   */
  void Run();

  /* ******************************************************************************************** */
  //! Variables

  Callback on_expire_;                  //!< Notify owner that message has expired
  std::chrono::milliseconds duration_;  //!< Time that message stays visible

  mutable std::mutex mutex_;                        //!< Control access for internal resources
  std::condition_variable notifier_;                //!< Wake up thread on any change
  std::optional<std::string> text_;                 //!< Message content (while visible)
  std::chrono::steady_clock::time_point deadline_;  //!< When message must be hidden
  bool exit_ = false;                               //!< Flag to stop thread execution

  std::thread thread_;  //!< Thread to hide message (only created when first message is shown)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_FLASH_MESSAGE_H_
