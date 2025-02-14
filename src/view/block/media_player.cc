#include "view/block/media_player.h"

#include <cstdlib>
#include <sstream>
#include <utility>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "model/volume.h"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"
#include "view/base/keybinding.h"

namespace interface {

/* ********************************************************************************************** */

MediaPlayer::MediaPlayer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, model::BlockIdentifier::MediaPlayer,
            interface::Size{.width = 0, .height = kMaxRows}} {
  btn_play_ = Button::make_button_play([this]() {
    LOG("Handle on_click event on Play button");
    auto disp = GetDispatcher();

    // Send event to set focus on this block
    AskForFocus();

    if (IsPlaying()) {
      bool resume = song_.curr_info.state == model::Song::MediaState::Pause;
      auto event = resume ? interface::CustomEvent::ResumeSong(/*run_animation=*/true)
                          : interface::CustomEvent::PauseSong();
      disp->SendEvent(event);
      return true;
    }

    // This event must be handled by ListDirectory, in case the selected file is an audio file, it
    // will start playing it
    auto event = interface::CustomEvent::PlaySong();
    disp->SendEvent(event);

    return false;
  });

  btn_stop_ = Button::make_button_stop([this]() {
    if (IsPlaying()) {
      auto disp = GetDispatcher();

      LOG("Handle on_click event on Stop button");
      auto event = interface::CustomEvent::StopSong();
      disp->SendEvent(event);

      // Send event to set focus on this block
      AskForFocus();

      return true;
    }
    return false;
  });

  btn_previous_ = Button::make_button_skip_previous([this]() {
    if (IsPlaying()) {
      auto disp = GetDispatcher();

      LOG("Handle on_click event on Skip to Previous Song button");
      auto event = interface::CustomEvent::SkipToPreviousSong();
      disp->SendEvent(event);

      // Send event to set focus on this block
      AskForFocus();

      return true;
    }
    return false;
  });

  btn_next_ = Button::make_button_skip_next([this]() {
    if (IsPlaying()) {
      auto disp = GetDispatcher();

      LOG("Handle on_click event on Skip to Next Song button");
      auto event = interface::CustomEvent::SkipToNextSong();
      disp->SendEvent(event);

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
  ftxui::Decorator bar_style =
      is_duration_focused_
          ? ftxui::bgcolor(ftxui::Color::LightSteelBlue1) | ftxui::color(ftxui::Color::RedLight)
          : ftxui::bgcolor(ftxui::Color::LightSteelBlue3) | ftxui::color(ftxui::Color::SteelBlue3);

  ftxui::Element bar_duration =
      ftxui::gauge(position) | ftxui::xflex_grow | ftxui::reflect(duration_box_) | bar_style;

  // Format volume information string
  std::ostringstream ss;
  ss << "Volume: " << std::setfill(' ') << std::setw(3) << ((int)volume_) << "%";
  std::string vol_info = std::move(ss).str();

  // Current volume element
  ftxui::Element volume = ftxui::text(vol_info);
  if (!volume_.IsMuted())
    volume |= ftxui::color(ftxui::Color::White);
  else
    volume |= ftxui::dim | ftxui::color(ftxui::Color::Red3Bis);

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
          btn_previous_->Render(),
          btn_play_->Render(),
          btn_stop_->Render(),
          btn_next_->Render(),
          ftxui::filler(),
          ftxui::vbox({
              ftxui::filler(),
              volume,
          }),
          margin,
      }),
      ftxui::text(""),
      ftxui::hbox({
          margin,
          bar_duration,
          margin,
      }),
      ftxui::hbox({
          margin,
          ftxui::text(curr_time) | ftxui::bold | ftxui::color(ftxui::Color::White),
          ftxui::filler(),
          ftxui::text(total_time) | ftxui::bold | ftxui::color(ftxui::Color::White),
          margin,
      }),
  });

  using ftxui::EQUAL;
  using ftxui::HEIGHT;

  return ftxui::window(
             ftxui::hbox(ftxui::text(" player ") | GetTitleDecorator()),
             content | ftxui::vcenter | ftxui::flex | ftxui::size(HEIGHT, EQUAL, kMaxRows)) |
         GetBorderDecorator();
}

/* ********************************************************************************************** */

bool MediaPlayer::OnEvent(ftxui::Event event) {
  if (event.is_mouse()) return OnMouseEvent(event);

  if (HandleMediaEvent(event)) return true;

  if (HandleVolumeEvent(event)) return true;

  if (HandleSeekEvent(event)) return true;

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

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongState) {
    song_.curr_info = event.GetContent<model::Song::CurrentInformation>();
    if (song_.curr_info.state == model::Song::MediaState::Play) btn_play_->SetState(true);
  }

  return false;
}

/* ********************************************************************************************** */

bool MediaPlayer::OnMouseEvent(ftxui::Event event) {
  // Media buttons
  if (btn_previous_->OnMouseEvent(event)) return true;
  if (btn_play_->OnMouseEvent(event)) return true;
  if (btn_stop_->OnMouseEvent(event)) return true;
  if (btn_next_->OnMouseEvent(event)) return true;

  if (!IsPlaying()) return false;

  // Mouse focus on song duration box
  is_duration_focused_ = duration_box_.Contain(event.mouse().x, event.mouse().y) ? true : false;

  // Mouse click on song duration box
  if (event.mouse().button == ftxui::Mouse::Left &&
      duration_box_.Contain(event.mouse().x, event.mouse().y)) {
    // Acquire pointer to dispatcher
    auto dispatcher = GetDispatcher();

    // Calculate new song position based on screen coordinates
    int real_x = event.mouse().x - duration_box_.x_min;
    auto new_position =
        (int)floor(floor(song_.duration * real_x) / (duration_box_.x_max - duration_box_.x_min));

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

/* ********************************************************************************************** */

bool MediaPlayer::HandleMediaEvent(const ftxui::Event& event) const {
  // Play a song or pause/resume current song
  if (event == keybinding::MediaPlayer::PlayOrPause) {
    LOG("Handle key to play/pause song");
    auto dispatcher = GetDispatcher();

    interface::CustomEvent event_play;

    if (!IsPlaying()) {
      event_play = interface::CustomEvent::PlaySong();
    } else {
      bool resume = song_.curr_info.state == model::Song::MediaState::Pause;
      event_play = resume ? interface::CustomEvent::ResumeSong(/*run_animation=*/true)
                          : interface::CustomEvent::PauseSong();
    }
    dispatcher->SendEvent(event_play);

    if (IsPlaying()) btn_play_->ToggleState();

    return true;
  }

  // Stop current song
  if (event == keybinding::MediaPlayer::Stop && IsPlaying()) {
    LOG("Handle key to stop current song");
    auto dispatcher = GetDispatcher();

    auto event_stop = interface::CustomEvent::StopSong();
    dispatcher->SendEvent(event_stop);

    return true;
  }

  if (event == keybinding::MediaPlayer::SkipToPrevious && IsPlaying()) {
    LOG("Handle key to skip to previous song");
    auto dispatcher = GetDispatcher();

    auto event_skip = interface::CustomEvent::SkipToPreviousSong();
    dispatcher->SendEvent(event_skip);

    btn_play_->ResetState();

    return true;
  }

  if (event == keybinding::MediaPlayer::SkipToNext && IsPlaying()) {
    LOG("Handle key to skip to next song");
    auto dispatcher = GetDispatcher();

    auto event_skip = interface::CustomEvent::SkipToNextSong();
    dispatcher->SendEvent(event_skip);

    btn_play_->ResetState();

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool MediaPlayer::HandleVolumeEvent(const ftxui::Event& event) {
  // Increase volume
  if (event == keybinding::MediaPlayer::VolumeUp) {
    LOG("Handle key to increase volume");
    auto dispatcher = GetDispatcher();

    auto old_value = volume_;
    volume_++;

    if (old_value != volume_) {
      auto event_volume = interface::CustomEvent::SetAudioVolume(volume_);
      dispatcher->SendEvent(event_volume);

      return true;
    }
  }

  // Decrease volume
  if (event == keybinding::MediaPlayer::VolumeDown) {
    LOG("Handle key to decrease volume");
    auto dispatcher = GetDispatcher();

    auto old_value = volume_;
    volume_--;

    if (old_value != volume_) {
      auto event_volume = interface::CustomEvent::SetAudioVolume(volume_);
      dispatcher->SendEvent(event_volume);

      return true;
    }
  }

  // Toggle volume mute
  if (event == keybinding::MediaPlayer::Mute) {
    LOG("Handle key to mute/unmute volume");
    auto dispatcher = GetDispatcher();

    volume_.ToggleMute();

    auto event_mute = interface::CustomEvent::SetAudioVolume(volume_);
    dispatcher->SendEvent(event_mute);

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool MediaPlayer::HandleSeekEvent(const ftxui::Event& event) const {
  // Seek forward in current song
  if (event == keybinding::MediaPlayer::SeekForward && IsPlaying()) {
    LOG("Handle key to seek forward in current song");
    auto dispatcher = GetDispatcher();

    // Since latest FFmpeg update, must increment by 2, instead of 1...
    auto event_seek = interface::CustomEvent::SeekForwardPosition(2);
    dispatcher->SendEvent(event_seek);

    return true;
  }

  // Seek backward in current song
  if (event == keybinding::MediaPlayer::SeekBackward && IsPlaying()) {
    LOG("Handle key to seek backward in current song");
    auto dispatcher = GetDispatcher();

    auto event_seek = interface::CustomEvent::SeekBackwardPosition(1);
    dispatcher->SendEvent(event_seek);

    return true;
  }

  return false;
}

}  // namespace interface
