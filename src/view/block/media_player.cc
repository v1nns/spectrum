#include "view/block/media_player.h"

#include <cstdlib>
#include <sstream>
#include <utility>  // for move
#include <vector>   // for vector

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"  // for Event
#include "model/volume.h"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"

namespace interface {

/* ********************************************************************************************** */

MediaPlayer::MediaPlayer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, Identifier::MediaPlayer, interface::Size{.width = 0, .height = kMaxRows}},
      btn_play_{nullptr},
      btn_stop_{nullptr},
      song_{},
      volume_{},
      duration_box_{} {
  btn_play_ = Button::make_button_play([&]() {
    // TODO: Try to play active entry from list directory
    if (IsPlaying()) {
      auto dispatcher = dispatcher_.lock();
      if (dispatcher) {
        auto event = interface::CustomEvent::PauseOrResumeSong();
        dispatcher->SendEvent(event);
      }
    }
  });

  btn_stop_ = Button::make_button_stop([&]() {
    if (IsPlaying()) {
      auto dispatcher = dispatcher_.lock();
      if (dispatcher) {
        auto event = interface::CustomEvent::StopSong();
        dispatcher->SendEvent(event);
      }
    }
  });
}

/* ********************************************************************************************** */

ftxui::Element MediaPlayer::Render() {
  // Duration
  std::string curr_time = "--:--";
  std::string total_time = "--:--";
  float position = 0;

  // Only fill these fields when exists a current song playing
  if (IsPlaying() || song_.duration > 0) {
    position = (float)song_.curr_info.position / (float)song_.duration;
    curr_time = model::time_to_string(song_.curr_info.position);
    total_time = model::time_to_string(song_.duration);
  }

  // Bar to display song duration
  ftxui::Element bar_duration =
      ftxui::gauge(position) | ftxui::xflex_grow | ftxui::reflect(duration_box_) |
      ftxui::bgcolor(ftxui::Color::LightSteelBlue3) | ftxui::color(ftxui::Color::SteelBlue3);

  // Format volume information string
  std::ostringstream ss;
  ss << "Volume: " << std::setfill(' ') << std::setw(3) << ((int)volume_) << "%";
  std::string vol_info = std::move(ss).str();

  // Current volume element
  ftxui::Element volume = ftxui::text(vol_info);

  // Fixed margin for content
  ftxui::Element margin = ftxui::text(std::string(5, ' '));

  // In order to maintain media buttons centered on screen, it is necessary to append this dummy
  // margin based on volume string length
  auto dummy_margin = ftxui::text(std::string(vol_info.size(), ' '));

  ftxui::Element content = ftxui::vbox({
      ftxui::hbox({
          margin,
          dummy_margin,
          ftxui::filler(),
          std::move(btn_play_->Render()),
          std::move(btn_stop_->Render()),
          ftxui::filler(),
          ftxui::vbox({
              ftxui::filler(),
              std::move(volume),
          }),
          margin,
      }),
      ftxui::text(""),
      ftxui::hbox({
          margin,
          std::move(bar_duration),
          margin,
      }),
      ftxui::hbox({
          margin,
          ftxui::text(curr_time) | ftxui::bold,
          ftxui::filler(),
          ftxui::text(total_time) | ftxui::bold,
          margin,
      }),
  });

  using ftxui::HEIGHT, ftxui::EQUAL;
  return ftxui::window(ftxui::text(" player "), content | ftxui::vcenter | ftxui::flex |
                                                    ftxui::size(HEIGHT, EQUAL, kMaxRows));
}

/* ********************************************************************************************** */

bool MediaPlayer::OnEvent(ftxui::Event event) {
  if (event.is_mouse()) return OnMouseEvent(event);

  // Clear current song
  if (event == ftxui::Event::Character('c') && IsPlaying()) {
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      auto event = interface::CustomEvent::ClearCurrentSong();
      dispatcher->SendEvent(event);

      btn_play_->ResetState();
    }

    return true;
  }

  // Pause or resume current song
  if (event == ftxui::Event::Character('p') && IsPlaying()) {
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      auto event = interface::CustomEvent::PauseOrResumeSong();
      dispatcher->SendEvent(event);

      btn_play_->ToggleState();
    }

    return true;
  }

  // Stop current song
  if (event == ftxui::Event::Character('s') && IsPlaying()) {
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      auto event = interface::CustomEvent::StopSong();
      dispatcher->SendEvent(event);

      btn_stop_->ToggleState();
    }

    return true;
  }

  // Decrease volume
  if (event == ftxui::Event::Character('-')) {
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      volume_--;

      auto event = interface::CustomEvent::SetAudioVolume(volume_);
      dispatcher->SendEvent(event);
    }

    return true;
  }

  // Increase volume
  if (event == ftxui::Event::Character('+')) {
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      volume_++;

      auto event = interface::CustomEvent::SetAudioVolume(volume_);
      dispatcher->SendEvent(event);
    }

    return true;
  }

  // Seek forward in current song
  if (event == ftxui::Event::Character('f') && IsPlaying()) {
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      auto event = interface::CustomEvent::SeekForwardPosition(1);
      dispatcher->SendEvent(event);
    }

    return true;
  }

  // Seek backward in current song
  if (event == ftxui::Event::Character('b') && IsPlaying()) {
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      auto event = interface::CustomEvent::SeekBackwardPosition(1);
      dispatcher->SendEvent(event);
    }

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool MediaPlayer::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::UpdateVolume) {
    volume_ = event.GetContent<model::Volume>();

    return true;
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::ClearSongInfo) {
    song_ = model::Song{.curr_info = {.state = model::Song::MediaState::Empty}};
    btn_play_->ResetState();
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    song_ = event.GetContent<model::Song>();
  }

  if (event == CustomEvent::Identifier::UpdateSongState) {
    song_.curr_info = event.GetContent<model::Song::CurrentInformation>();
    if (song_.curr_info.state == model::Song::MediaState::Play) btn_play_->SetState(true);

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool MediaPlayer::OnMouseEvent(ftxui::Event event) {
  if (btn_play_->OnEvent(event)) return true;

  if (btn_stop_->OnEvent(event)) return true;

  // Mouse click on duration box
  if (IsPlaying() && event.mouse().button == ftxui::Mouse::Left &&
      duration_box_.Contain(event.mouse().x, event.mouse().y)) {
    // Acquire pointer to dispatcher
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return true;

    // Calculate new song position based on screen coordinates
    int real_x = event.mouse().x - duration_box_.x_min;
    int new_position =
        floor(floor(song_.duration * real_x) / (duration_box_.x_max - duration_box_.x_min));

    int offset = std::abs(int(new_position - song_.curr_info.position));

    // Do nothing if result is equal the current position
    if (new_position == song_.curr_info.position) return true;

    // Send event to player
    interface::CustomEvent event;

    if (new_position > song_.curr_info.position)
      event = interface::CustomEvent::SeekForwardPosition(offset);
    else
      event = interface::CustomEvent::SeekBackwardPosition(offset);

    dispatcher->SendEvent(event);
    return true;
  }

  return false;
}

}  // namespace interface
