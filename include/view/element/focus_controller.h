/**
 * \file
 * \brief  Class wrapper to control focus on a list of elements
 */

#ifndef INCLUDE_VIEW_ELEMENT_FOCUS_CONTROLLER_H_
#define INCLUDE_VIEW_ELEMENT_FOCUS_CONTROLLER_H_

#include "view/base/element.h"
#include "view/base/keybinding.h"

namespace interface {

//! Wrapper to control focus on elements based on external events
// NOTE: in this first version, this class is considering only horizontal direction
class FocusController final {
  using Keybinding = keybinding::Navigation;
  static constexpr int kInvalidIndex = -1;  //!< Invalid index (when no element is focused)

  /* ******************************************************************************************** */
  //! Public API
 public:
  /**
   * @brief Append elements to manage focus (using C++17 variadic template)
   *        P.S.: Focus priority is based on elements order in internal vector
   * @tparam ...Args Elements
   * @param ...args Elements to be appended
   */
  template <typename... Args>
  void Append(Args&... args) {
    elements_.insert(elements_.end(), {static_cast<decltype(elements_)::value_type>(&args)...});
  }

  /**
   * @brief Append array of elements to manage focus (using SFINAE to enable only for iterators)
   *        P.S.: Focus priority is based on elements order in internal vector
   * @tparam Iterator Element container iterator
   * @param begin Initial element
   * @param end Last element
   */
  template <class Iterator,
            typename = std::is_same<typename std::iterator_traits<Iterator>::iterator_category,
                                    std::input_iterator_tag>>
  void Append(Iterator begin, Iterator end) {
    auto size = std::distance(begin, end);
    elements_.reserve(elements_.size() + size);

    for (auto it = begin; it != end; it++) elements_.push_back(&*it);
  }

  /**
   * @brief Handles an event (from keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event);

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEvent(ftxui::Event& event);

  /**
   * @brief Change index of focused element
   * @param index Element index
   */
  void SetFocus(int index);

  /* ******************************************************************************************** */
  //! Internal implementation
 private:
  /**
   * @brief Update focus state in both old and newly focused elements
   * @param old_index Element index with focus
   * @param new_index Element index to be focused
   */
  void UpdateFocus(int old_index, int new_index);

  /**
   * @brief Check if contains any element focused
   * @return True if any element is focused, otherwise false
   */
  bool HasElementFocused() const { return focus_index_ != kInvalidIndex; }

  /* ******************************************************************************************** */
  //! Variables

  std::vector<Element*> elements_;   //!< List of elements ordered by focus priority
  int focus_index_ = kInvalidIndex;  //!< Index to current element focused

  //!< List of mapped events to be handled as action key
  const std::array<ftxui::Event, 6> action_events{Keybinding::ArrowUp, Keybinding::ArrowDown,
                                                  Keybinding::Up,      Keybinding::Down,
                                                  Keybinding::Space,   Keybinding::Return};
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_FOCUS_CONTROLLER_H_
