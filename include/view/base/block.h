/**
 * \file
 * \brief  Base class for any content block displayed in the UI
 */

#ifndef INCLUDE_VIEW_BASE_BLOCK_H_
#define INCLUDE_VIEW_BASE_BLOCK_H_

#include <memory>   // for shared_ptr, enable_sha...
#include <string>   // for string, operator==
#include <utility>  // for move

#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "model/block_identifier.h"
#include "view/base/custom_event.h"

namespace interface {

//! Forward declaration
class EventDispatcher;

//! Maximum dimensions for block
struct Size {
  int width;   //!< Maximum width for block
  int height;  //!< Maximum height for block
};

/**
 * @brief Base class representing a block in view
 */
class Block : public std::enable_shared_from_this<Block>, public ftxui::ComponentBase {
 protected:
  /**
   * @brief Construct a new Block object (only called by derived classes)
   * @param dispatcher Event dispatcher
   * @param id Unique ID for block
   * @param size Block dimensions
   */
  Block(const std::shared_ptr<EventDispatcher>& dispatcher, const model::BlockIdentifier& id,
        const Size& size);

 public:
  /**
   * @brief Destroy the Block object
   */
  ~Block() override = default;

  //! Unique ID
  model::BlockIdentifier GetId() const { return id_; }

  //! Block size
  Size GetSize() const { return size_; }

  //! Set focus state
  void SetFocused(bool focused);

  //! Get focus state
  bool IsFocused() const { return focused_; }

 protected:
  //! Get decorator style for title based on internal state
  ftxui::Decorator GetTitleDecorator() const;

  //! Get decorator style for border based on internal state
  ftxui::Decorator GetBorderDecorator() const;

  //! Dispatch event to set focus
  void AskForFocus() const;

  /* ******************************************************************************************** */
  //! These must be implemented by derived class
 public:

  ftxui::Element Render() override { return ftxui::Element(); }
  bool OnEvent(ftxui::Event) override { return false; }
  virtual bool OnCustomEvent(const CustomEvent&) = 0;

  /* ******************************************************************************************** */
  //! These have optional implementation by derived class

  virtual void OnFocus() {
    // This method is called whenever block is focused
  }

  virtual void OnLostFocus() {
    // This method is called whenever block loses focus
  }

  /* ******************************************************************************************** */
  //! Used by derived class
 protected:
  //! Get event dispatcher
  std::shared_ptr<EventDispatcher> GetDispatcher() const;

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::weak_ptr<EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks
  model::BlockIdentifier id_;                  //!< Block identification
  Size size_;                                  //!< Block size
  bool focused_ = false;  //!< Control flag for focus state, to help with UI navigation
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_BLOCK_H_
