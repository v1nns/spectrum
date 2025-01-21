/**
 * \file
 * \brief  Class for rendering a customized menu helper
 */

#ifndef INCLUDE_VIEW_ELEMENT_HELP_H_
#define INCLUDE_VIEW_ELEMENT_HELP_H_

#include "view/base/dialog.h"

namespace interface {

/**
 * @brief Customized dialog box to show a helper
 */
class HelpDialog : public Dialog {
  static constexpr int kMaxColumns = 90;  //!< Maximum columns for Element
  static constexpr int kMaxLines = 30;    //!< Maximum lines for Element

 public:
  /**
   * @brief Construct a new Help object
   */
  HelpDialog();

  /**
   * @brief Destroy Help object
   */
  ~HelpDialog() override = default;

  /* ******************************************************************************************** */
  //! Custom implementation
 private:
  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element RenderImpl(const ftxui::Dimensions& curr_size) const override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEventImpl(const ftxui::Event& event) override;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEventImpl(ftxui::Event event) override;

  /* ******************************************************************************************** */
  //! Public API
 public:
  /**
   * @brief Set dialog state to visible
   */
  void ShowGeneralInfo();

  /**
   * @brief Set dialog state to visible
   */
  void ShowTabInfo();

  /* ******************************************************************************************** */
  //! UI utilities
 private:
  /**
   * @brief Possible tab views to render on this block
   */
  enum class View {
    General,  //!< Display general info (default)
    Tab,      //!< Display tab info
    LAST,
  };

  /**
   * @brief Build UI component for title
   * @param message Content to show as title
   * @return User interface element
   */
  ftxui::Element title(const std::string& message) const;

  /**
   * @brief Build UI component for keybinding + command
   * @param keybind Keybinding option
   * @param description Command description
   * @return User interface element
   */
  ftxui::Element command(const std::string& keybind, const std::string& description) const;

  /**
   * @brief Build UI component for general block information
   * @return User interface element
   */
  ftxui::Element BuildGeneralInfo() const;

  /**
   * @brief Build UI component for tab information
   * @return User interface element
   */
  ftxui::Element BuildTabInfo() const;

  /* ******************************************************************************************** */
  //! Variables

  View active_ = View::General;  //!< Current view displayed on dialog
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_HELP_H_
