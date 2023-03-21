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
    : Block{dispatcher, model::BlockIdentifier::MediaPlayer,
            interface::Size{.width = 0, .height = kMaxRows}},
      btn_play_{nullptr},
      btn_stop_{nullptr},
      song_{},
      volume_{},
      duration_box_{} {
  btn_play_ = Button::make_button_play([&]() -> bool {
    LOG("Handle on_click event on Play button");
    auto dispatcher = GetDispatcher();

    // Send event to set focus on this block
    AskForFocus();

    if (IsPlaying()) {
      auto event = interface::CustomEvent::PauseOrResumeSong();
      dispatcher->SendEvent(event);
      return true;
    }

    // This event must be handled by ListDirectory, in case the selected file is an audio file, it
    // will start playing it
    auto event = interface::CustomEvent::PlaySong();
    dispatcher->SendEvent(event);

    return false;
  });

  btn_stop_ = Button::make_button_stop([&]() -> bool {
    if (IsPlaying()) {
      auto dispatcher = GetDispatcher();

      LOG("Handle on_click event on Stop button");
      auto event = interface::CustomEvent::StopSong();
      dispatcher->SendEvent(event);

      // Send event to set focus on this block
      AskForFocus();

      return true;
    }
    return false;
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
  if (volume_.IsMuted()) volume |= ftxui::dim | ftxui::color(ftxui::Color::Red3Bis);

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
  return ftxui::window(
      ftxui::hbox(ftxui::text(" player ") | GetTitleDecorator()),
      content | ftxui::vcenter | ftxui::flex | ftxui::size(HEIGHT, EQUAL, kMaxRows));
}

/* ********************************************************************************************** */

bool MediaPlayer::OnEvent(ftxui::Event event) {
  if (event.is_mouse()) return OnMouseEvent(event);

  // Clear current song
  if (event == ftxui::Event::Character('c') && IsPlaying()) {
    LOG("Handle key to clear current song");
    auto dispatcher = GetDispatcher();

    auto event = interface::CustomEvent::ClearCurrentSong();
    dispatcher->SendEvent(event);

    btn_play_->ResetState();

    return true;
  }

  // Play a song or pause/resume current song
  if (event == ftxui::Event::Character('p')) {
    LOG("Handle key to play/pause song");
    auto dispatcher = GetDispatcher();

    auto event = !IsPlaying() ? interface::CustomEvent::PlaySong()
                              : interface::CustomEvent::PauseOrResumeSong();

    dispatcher->SendEvent(event);

    if (IsPlaying()) btn_play_->ToggleState();

    return true;
  }

  // Stop current song
  if (event == ftxui::Event::Character('s') && IsPlaying()) {
    LOG("Handle key to stop current song");
    auto dispatcher = GetDispatcher();

    auto event = interface::CustomEvent::StopSong();
    dispatcher->SendEvent(event);

    btn_stop_->ToggleState();

    return true;
  }

  // Decrease volume
  if (event == ftxui::Event::Character('-')) {
    LOG("Handle key to decrease volume");
    auto dispatcher = GetDispatcher();

    volume_--;

    auto event = interface::CustomEvent::SetAudioVolume(volume_);
    dispatcher->SendEvent(event);

    return true;
  }

  // Increase volume
  if (event == ftxui::Event::Character('+')) {
    LOG("Handle key to increase volume");
    auto dispatcher = GetDispatcher();

    volume_++;

    auto event = interface::CustomEvent::SetAudioVolume(volume_);
    dispatcher->SendEvent(event);

    return true;
  }

  // Toggle volume mute
  if (event == ftxui::Event::Character('m')) {
    LOG("Handle key to mute/unmute volume");
    auto dispatcher = GetDispatcher();

    volume_.ToggleMute();

    auto event = interface::CustomEvent::SetAudioVolume(volume_);
    dispatcher->SendEvent(event);

    return true;
  }

  // Seek forward in current song
  if (event == ftxui::Event::Character('f') && IsPlaying()) {
    LOG("Handle key to seek forward in current song");
    auto dispatcher = GetDispatcher();

    auto event = interface::CustomEvent::SeekForwardPosition(1);
    dispatcher->SendEvent(event);

    return true;
  }

  // Seek backward in current song
  if (event == ftxui::Event::Character('b') && IsPlaying()) {
    LOG("Handle key to seek backward in current song");
    auto dispatcher = GetDispatcher();

    auto event = interface::CustomEvent::SeekBackwardPosition(1);
    dispatcher->SendEvent(event);

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool MediaPlayer::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::UpdateVolume) {
    LOG("Received new volume information from player");
    volume_ = event.GetContent<model::Volume>();

    return true;
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    song_ = model::Song{.curr_info = {.state = model::Song::MediaState::Empty}};
    btn_play_->ResetState();
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");
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
    auto dispatcher = GetDispatcher();

    // Calculate new song position based on screen coordinates
    int real_x = event.mouse().x - duration_box_.x_min;
    int new_position =
        floor(floor(song_.duration * real_x) / (duration_box_.x_max - duration_box_.x_min));

    int offset = std::abs(int(new_position - song_.curr_info.position));

    // Do nothing if result is equal the current position
    if (new_position == song_.curr_info.position) return true;

    LOG("Handle left click mouse event on song progress bar");

    // Send event to player
    interface::CustomEvent event_seek = new_position > song_.curr_info.position
                                            ? interface::CustomEvent::SeekForwardPosition(offset)
                                            : interface::CustomEvent::SeekBackwardPosition(offset);

    LOG("Sending event to ", event_seek.GetId(), " with offset=", offset);
    dispatcher->SendEvent(event_seek);

    // Set this block as active (focused)
    auto event_focus = interface::CustomEvent::SetFocused(GetId());
    dispatcher->SendEvent(event_focus);

    return true;
  }

  return false;
}

}  // namespace interface
