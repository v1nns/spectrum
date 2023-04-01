#include "view/base/block.h"

#include "util/logger.h"
#include "view/base/event_dispatcher.h"

namespace interface {

Block::Block(const std::shared_ptr<EventDispatcher>& dispatcher, const model::BlockIdentifier& id,
             const Size& size)
    : ftxui::ComponentBase{}, dispatcher_{dispatcher}, id_{id}, size_{size} {}

/* ********************************************************************************************** */

void Block::SetFocused(bool focused) { focused_ = focused; }

/* ********************************************************************************************** */

ftxui::Decorator Block::GetTitleDecorator() const {
  using ftxui::bgcolor;
  using ftxui::bold;
  using ftxui::Color;
  using ftxui::color;

  ftxui::Decorator style = focused_ ? bgcolor(Color::DodgerBlue1) | color(Color::DarkBlue) | bold
                                    : bgcolor(Color::GrayDark) | color(Color::GrayLight);

  return style;
}

/* ********************************************************************************************** */

void Block::AskForFocus() const {
  auto dispatcher = GetDispatcher();

  // Set this block as active (focused)
  auto event = interface::CustomEvent::SetFocused(id_);
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

std::shared_ptr<EventDispatcher> Block::GetDispatcher() const {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) {
    ERROR("Cannot lock event dispatcher");
    throw std::runtime_error("Cannot lock event dispatcher");
  }

  return dispatcher;
}

}  // namespace interface
