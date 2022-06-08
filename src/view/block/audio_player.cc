#include "view/block/audio_player.h"

#include <utility>  // for move
#include <vector>   // for vector

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"  // for Event

namespace interface {

constexpr int kMaxRows = 10;  //!< Maximum rows for the Component

/* ********************************************************************************************** */

AudioPlayer::AudioPlayer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, kBlockAudioPlayer}, btn_play_{nullptr}, btn_stop_{nullptr} {
  // TODO: bind methods for on_click
  btn_play_ = Button::make_button_play(nullptr);
  btn_stop_ = Button::make_button_stop(nullptr);
}

/* ********************************************************************************************** */

ftxui::Element AudioPlayer::Render() {
  // Duration
  ftxui::Element bar_duration = ftxui::gauge(0.9) | ftxui::xflex_grow |
                                ftxui::bgcolor(ftxui::Color::DarkKhaki) |
                                ftxui::color(ftxui::Color::DarkVioletBis);

  ftxui::Element bar_margin = ftxui::text("  ");

  ftxui::Element content = ftxui::vbox({ftxui::hbox({
                                            std::move(btn_play_->Render()),
                                            std::move(btn_stop_->Render()),
                                        }) | ftxui::center,
                                        ftxui::text(""),
                                        ftxui::hbox({
                                            bar_margin,
                                            std::move(bar_duration),
                                            bar_margin,
                                        }),
                                        ftxui::hbox({
                                            bar_margin,
                                            ftxui::text("1:38") | ftxui::bold,
                                            ftxui::filler(),
                                            ftxui::text("1:58") | ftxui::bold,
                                            bar_margin,
                                        })});

  using ftxui::HEIGHT, ftxui::EQUAL;
  return ftxui::window(ftxui::text(" player "), content | ftxui::vcenter | ftxui::flex |
                                                    ftxui::size(HEIGHT, EQUAL, kMaxRows));
}

/* ********************************************************************************************** */

bool AudioPlayer::OnEvent(ftxui::Event event) {
  if (event.is_mouse()) return OnMouseEvent(event);

  // if(Focusable()){}

  return false;
}

/* ********************************************************************************************** */

bool AudioPlayer::OnMouseEvent(ftxui::Event event) {
  if (btn_play_->OnEvent(event)) return true;

  if (btn_stop_->OnEvent(event)) return true;

  return false;
}

}  // namespace interface
