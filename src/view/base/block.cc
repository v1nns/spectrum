#include "view/base/block.h"

#include <utility>

#include "util/logger.h"
#include "view/base/event_dispatcher.h"
#include "view/element/style.h"

namespace interface {

Block::Block(const std::shared_ptr<EventDispatcher>& dispatcher, const model::BlockIdentifier& id,
             const Size& size)
    : ftxui::ComponentBase{}, dispatcher_{dispatcher}, id_{id}, size_{size} {}

/* ********************************************************************************************** */

void Block::SetFocused(bool focused) {
  focused_ = focused;

  if (focused_)
    OnFocus();
  else
    OnLostFocus();
}

/* ********************************************************************************************** */

ftxui::Decorator Block::GetTitleDecorator() const {
  using ftxui::bgcolor;
  using ftxui::bold;
  using ftxui::color;

  const auto& theme = GetTheme().block;
  const auto& title = focused_ ? theme.title_focused : theme.title;

  ftxui::Decorator decorator = bgcolor(title.background) | color(title.foreground);
  return focused_ ? decorator | bold : decorator;
}

/* ********************************************************************************************** */

ftxui::Decorator Block::GetBorderDecorator() const {
  using ftxui::color;

  const auto& theme = GetTheme().block;
  return color(focused_ ? theme.border_focused : theme.border);
}

/* ********************************************************************************************** */

ftxui::Decorator Block::GetContentDecorator() const {
  // Without focus, border uses a color of its own, so do not let anything inside inherit it
  // (as it happens with any text without a color)
  return focused_ ? ftxui::nothing : ftxui::color(GetTheme().screen.foreground);
}

/* ********************************************************************************************** */

ftxui::Element Block::RenderWindow(ftxui::Element title, ftxui::Element content) const {
  // Title is not decorated here, otherwise it would also change the border line around it
  return ftxui::window(std::move(title), std::move(content) | GetContentDecorator()) |
         GetBorderDecorator();
}

/* ********************************************************************************************** */

void Block::AskForFocus() const {
  if (focused_) return;

  auto dispatcher = GetDispatcher();

  // Set this block as active (focused)
  auto event = interface::CustomEvent::SetFocused(id_);
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

std::shared_ptr<EventDispatcher> Block::GetDispatcher() const {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) {
    WARN("Cannot lock event dispatcher");
    throw std::runtime_error("Cannot lock event dispatcher");
  }

  return dispatcher;
}

}  // namespace interface
