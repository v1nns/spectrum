/**
 * \file
 * \brief  Class for rendering a text input to type an URL
 */

#ifndef INCLUDE_VIEW_ELEMENT_URL_INPUT_H_
#define INCLUDE_VIEW_ELEMENT_URL_INPUT_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "view/base/element.h"
#include "view/element/text_input.h"

namespace interface {

/**
 * @brief Text input to type (or paste) an URL, showing feedback after it is submitted
 */
class UrlInput : public Element {
  static constexpr int kPadding = 1;  //!< Columns left empty on each side of content

 public:
  //! Result from submitted URL
  struct Result {
    //! Possible outcomes
    enum class Status : std::uint8_t {
      Accepted,  //!< URL was accepted (text input is cleared)
      Rejected,  //!< URL was rejected (text is kept, so user can fix it)
      Pending,   //!< URL is still being handled (result is set later, using SetResult)
    };

    Status status;        //!< Outcome
    std::string message;  //!< Message to display (if empty for accepted URL, default one is used)
  };

  /**
   * @brief Callback triggered when URL is submitted
   * @param url URL typed by user
   * @return Result from URL
   */
  using Callback = std::function<Result(const std::string& url)>;

  /**
   * @brief Construct a new UrlInput object
   * @param label Text displayed above input
   * @param success Message displayed after URL is accepted
   * @param on_submit Callback triggered when URL is submitted
   */
  UrlInput(const std::string& label, const std::string& success, const Callback& on_submit);

  /**
   * @brief Destroy UrlInput object
   */
  ~UrlInput() override = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Renders the element
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event) override;

  /**
   * @brief Set maximum number of columns available to render text input
   * @param max_columns Number of columns
   */
  void SetMaxColumns(int max_columns) { max_columns_ = max_columns; }

  /**
   * @brief Set text displayed above input
   * @param label Label text
   */
  void SetLabel(const std::string& label) { label_ = label; }

  /**
   * @brief Clear typed text and feedback message
   */
  void Clear();

  /**
   * @brief Set result from URL submitted earlier (e.g. after a pending operation has finished)
   * @param result Result from URL
   */
  void SetResult(const Result& result);

  /* ******************************************************************************************** */
  //! Internal implementation
 private:
  //! Submit typed text to owner
  void Submit();

  /* ******************************************************************************************** */
  //! Variables

  std::string label_;    //!< Text displayed above input
  std::string success_;  //!< Message displayed after URL is accepted
  Callback on_submit_;   //!< Callback triggered when URL is submitted

  TextInput input_;                 //!< Text input to type URL
  int max_columns_ = 0;             //!< Maximum columns available to render text input
  std::optional<Result> feedback_;  //!< Feedback from last submitted URL
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_URL_INPUT_H_
