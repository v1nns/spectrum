#include "view/base/block.h"

#include "util/logger.h"
#include "view/base/event_dispatcher.h"

namespace interface {

Block::Block(const std::shared_ptr<EventDispatcher>& dispatcher, const model::BlockIdentifier& id,
             const Size& size)
    : ftxui::ComponentBase{}, id_{id}, dispatcher_{dispatcher}, size_{size}, focused_{false} {}

/* ********************************************************************************************** */

void Block::SetFocused(bool focused) { focused_ = focused; }

/* ********************************************************************************************** */

ftxui::Decorator Block::GetTitleDecorator() {
  using ftxui::Color, ftxui::bgcolor, ftxui::color, ftxui::bold;

  ftxui::Decorator style = focused_ ? bgcolor(Color::DodgerBlue1) | color(Color::DarkBlue) | bold
                                    : bgcolor(Color::GrayDark) | color(Color::GrayLight);

  return style;
}

}  // namespace interface
