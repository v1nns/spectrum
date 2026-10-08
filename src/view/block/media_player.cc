#include "view/block/media_player.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include "model/volume.h"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"
#include "view/element/util.h"

namespace interface {

//! Volume level is shown (and saved) as percentage
static constexpr float kMaxVolume = 100.F;

namespace {

/**
 * @brief Horizontal line filled according to a progress, using as many columns as available (a
 * heavy line for the part already filled and a light one for the remaining part)
 */
class ProgressLine : public ftxui::Node {
 public:
  /**
   * @brief Construct a new line
   * @param progress Value from 0 (empty) to 1 (full)
   * @param colors Foreground for filled part and background for remaining part
   * @param show_knob Draw a knob on the current position
   */
  ProgressLine(float progress, const Theme::State& colors, bool show_knob)
      : progress_{std::clamp(progress, 0.F, 1.F)}, colors_{colors}, show_knob_{show_knob} {}

  void ComputeRequirement() override {
    requirement_.min_x = 1;
    requirement_.min_y = 1;
  }

  void Render(ftxui::Screen& screen) override {
    const int width = box_.x_max - box_.x_min + 1;
    if (width <= 0) return;

    const int filled = static_cast<int>(std::round(progress_ * static_cast<float>(width)));
    const int knob = std::min(filled, width - 1);

    for (int i = 0; i < width; i++) {
      auto& pixel = screen.PixelAt(box_.x_min + i, box_.y_min);
      const bool is_filled = i < filled;

      pixel.character = show_knob_ && i == knob ? "●" : is_filled ? "━" : "─";
      pixel.foreground_color =
          is_filled || (show_knob_ && i == knob) ? colors_.foreground : colors_.background;
    }
  }

 private:
  float progress_;       //!< Value from 0 to 1
  Theme::State colors_;  //!< Colors for filled and remaining parts
  bool show_knob_;       //!< Draw a knob on current position
};

/**
 * @brief Text in a single line, ending with an ellipsis when there is not enough space for it
 */
class EllipsizedText : public ftxui::Node {
 public:
  explicit EllipsizedText(std::string text) : text_{std::move(text)} {}

  void ComputeRequirement() override {
    requirement_.min_x = ftxui::string_width(text_);
    requirement_.min_y = 1;
  }

  void Render(ftxui::Screen& screen) override {
    if (box_.y_min > box_.y_max) return;

    const int width = box_.x_max - box_.x_min + 1;
    int x = box_.x_min;

    // A glyph using more than one column is followed by empty cells, one for each extra column
    for (const auto& cell : ftxui::Utf8ToGlyphs(ellipsize(text_, width))) {
      if (x > box_.x_max) return;
      screen.PixelAt(x++, box_.y_min).character = cell;
    }
  }

 private:
  std::string text_;  //!< Whole text
};

//! Create a text that is cut (ending with an ellipsis) when it gets less space than it needs
ftxui::Element ellipsized_text(const std::string& text) {
  return std::make_shared<EllipsizedText>(text);
}

//! Create a line filled according to the given progress
ftxui::Element progress_line(float progress, const Theme::State& colors, bool show_knob = false) {
  return std::make_shared<ProgressLine>(progress, colors, show_knob);
}

}  // namespace

/* ********************************************************************************************** */

MediaPlayer::MediaPlayer(const std::shared_ptr<EventDispatcher>& dispatcher,
                         const std::shared_ptr<util::FileHandler>& file_handler)
    : Block{dispatcher, model::BlockIdentifier::MediaPlayer,
            interface::Size{.width = 0, .height = kMaxRows}},
      warning_{[this] {
                 // Warning has expired, so UI must be refreshed to remove it from screen
                 if (auto disp = GetDispatcher(); disp) {
                   disp->SendEvent(CustomEvent::Refresh());
                 }
               },
               kWarningDuration},
      file_handler_{file_handler != nullptr ? file_handler
                                            : std::make_shared<util::FileHandler>()} {
  // Restore volume from last run, and let audio player know about it
  if (model::Settings settings; file_handler_->ParseSettings(settings) && settings.volume) {
    volume_ = model::Volume{static_cast<float>(*settings.volume) / 100.F};
    INFO("Restored volume=", volume_);

    if (auto disp = GetDispatcher(); disp) disp->SendEvent(CustomEvent::SetAudioVolume(volume_));
  }

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
      auto event = CreateSkipEvent(/*next=*/false);
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
      auto event = CreateSkipEvent(/*next=*/true);
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
  using ftxui::EQUAL;
  using ftxui::HEIGHT;
  using ftxui::WIDTH;

  const auto& theme = GetTheme().player;

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

  // Line to display song duration
  ftxui::Element line_duration =
      progress_line(position, is_duration_focused_ ? theme.duration_focused : theme.duration,
                    /*show_knob=*/IsPlaying()) |
      ftxui::xflex_grow | ftxui::reflect(duration_box_);

  // Song title and artist (cut when they do not fit in the space left by everything else)
  ftxui::Element title =
      ellipsized_text(GetSongTitle()) | ftxui::bold | ftxui::color(theme.text) | ftxui::xflex;

  ftxui::Element artist = ellipsized_text(song_.artist) | ftxui::dim | ftxui::xflex;

  // Repeat and shuffle modes (dimmed when disabled, with space around text to not change its
  // size when enabled)
  auto mode = [&theme](const std::string& text, bool enabled) {
    const auto& colors = theme.mode_enabled;

    return ftxui::text(" " + text + " ") |
           (enabled ? ftxui::color(colors.foreground) | ftxui::bgcolor(colors.background)
                    : ftxui::dim);
  };

  ftxui::Element modes = ftxui::hbox({
      mode(std::string{"shuffle "} + (shuffle_ ? "on" : "off"), shuffle_),
      mode("repeat " + std::string{model::GetRepeatModeName(repeat_)},
           repeat_ != model::RepeatMode::Off),
  });

  // Current volume, as a line and as a percentage
  std::ostringstream ss;
  ss << std::setfill(' ') << std::setw(4) << ((int)volume_) << "% ";

  ftxui::Element volume = ftxui::hbox({
      ftxui::text("vol ") | ftxui::dim,
      progress_line(static_cast<float>(volume_), theme.duration) |
          ftxui::size(WIDTH, EQUAL, kVolumeColumns),
      ftxui::text(std::move(ss).str()) | ftxui::color(theme.text),
  });

  if (volume_.IsMuted()) volume = volume | ftxui::dim | ftxui::color(theme.volume_muted);

  // Fixed margin for content
  ftxui::Element margin = ftxui::text(std::string(kMarginColumns, ' '));

  // Warning (if any) uses the empty line between song and media buttons
  ftxui::Element warning = ftxui::text("");
  if (auto message = warning_.GetText(); message.has_value()) {
    warning = ftxui::text(*message) | ftxui::bold | ftxui::color(theme.warning) | ftxui::center;
  }

  ftxui::Element content = ftxui::vbox({
      ftxui::hbox({
          margin,
          title,
          modes,
          margin,
      }),
      ftxui::hbox({
          margin,
          artist,
          ftxui::text(" "),
          volume,
          margin,
      }),
      ftxui::hbox({
          margin,
          warning | ftxui::xflex_grow,
          margin,
      }),
      ftxui::hbox({
          margin,
          btn_previous_->Render(),
          btn_play_->Render(),
          btn_stop_->Render(),
          btn_next_->Render(),
          margin,
          ftxui::text(curr_time) | ftxui::bold | ftxui::color(theme.text),
          ftxui::text(" "),
          line_duration,
          ftxui::text(" "),
          ftxui::text(total_time) | ftxui::bold | ftxui::color(theme.text),
          margin,
      }),
  });

  return RenderWindow(ftxui::hbox(ftxui::text(" player ") | GetTitleDecorator()),
                      content | ftxui::size(HEIGHT, EQUAL, kMaxRows));
}

/* ********************************************************************************************** */

std::string MediaPlayer::GetSongTitle() const {
  if (!song_.title.empty()) return song_.title;

  // Song is played from a file or streamed from URL
  if (!song_.filepath.empty()) return song_.filepath.filename().string();

  return song_.stream_info.has_value() ? song_.stream_info->base_url : std::string{};
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

CustomEvent MediaPlayer::CreateSkipEvent(bool next) {
  return next ? CustomEvent::SkipToNextPlaylistSong() : CustomEvent::SkipToPreviousPlaylistSong();
}

/* ********************************************************************************************** */

bool MediaPlayer::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::ShowWarning) {
    LOG("Received warning to show");
    warning_.Show(event.GetContent<std::string>());

    return true;
  }

  if (event == CustomEvent::Identifier::RunRemoteCommand) {
    HandleRemoteCommand(event.GetContent<model::RemoteRequest>());

    return true;
  }

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

bool MediaPlayer::HandleMediaEvent(const ftxui::Event& event) {
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

  if (event == keybinding::MediaPlayer::ToggleRepeat) {
    repeat_ = model::GetNextRepeatMode(repeat_);
    LOG("Handle key to change repeat mode to ", repeat_);

    auto dispatcher = GetDispatcher();
    dispatcher->SendEvent(interface::CustomEvent::SetRepeatMode(repeat_));
    return true;
  }

  if (event == keybinding::MediaPlayer::ToggleShuffle) {
    shuffle_ = !shuffle_;
    LOG("Handle key to toggle shuffle to ", shuffle_ ? "on" : "off");

    auto dispatcher = GetDispatcher();
    dispatcher->SendEvent(interface::CustomEvent::SetShuffle(shuffle_));
    return true;
  }

  if (event == keybinding::MediaPlayer::SkipToPrevious && IsPlaying()) {
    LOG("Handle key to skip to previous song");
    auto dispatcher = GetDispatcher();

    auto event_skip = CreateSkipEvent(/*next=*/false);
    dispatcher->SendEvent(event_skip);

    btn_play_->ResetState();

    return true;
  }

  if (event == keybinding::MediaPlayer::SkipToNext && IsPlaying()) {
    LOG("Handle key to skip to next song");
    auto dispatcher = GetDispatcher();

    auto event_skip = CreateSkipEvent(/*next=*/true);
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

      SaveVolume();
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

      SaveVolume();
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

/* ********************************************************************************************** */

void MediaPlayer::HandleRemoteCommand(const model::RemoteRequest& request) {
  LOG("Handle remote command=", request);
  if (HandleRemoteValue(request)) return;

  const model::RemoteCommand command = request.command;
  const keybinding::Key* key = nullptr;
  const bool playing = song_.curr_info.state == model::Song::MediaState::Play;

  switch (command) {
    case model::RemoteCommand::PlayOrPause:
      key = &keybinding::MediaPlayer::PlayOrPause;
      break;

    case model::RemoteCommand::Play:
      // Unlike the key, it does not toggle: song already playing is not paused
      if (!playing) key = &keybinding::MediaPlayer::PlayOrPause;
      break;

    case model::RemoteCommand::Pause:
      // Unlike the key, it does not toggle: song already paused is not resumed
      if (playing) key = &keybinding::MediaPlayer::PlayOrPause;
      break;

    case model::RemoteCommand::Stop:
      key = &keybinding::MediaPlayer::Stop;
      break;

    case model::RemoteCommand::SkipToPrevious:
      key = &keybinding::MediaPlayer::SkipToPrevious;
      break;

    case model::RemoteCommand::SkipToNext:
      key = &keybinding::MediaPlayer::SkipToNext;
      break;

    case model::RemoteCommand::VolumeUp:
      key = &keybinding::MediaPlayer::VolumeUp;
      break;

    case model::RemoteCommand::VolumeDown:
      key = &keybinding::MediaPlayer::VolumeDown;
      break;

    case model::RemoteCommand::Mute:
      key = &keybinding::MediaPlayer::Mute;
      break;

    case model::RemoteCommand::SeekForward:
      key = &keybinding::MediaPlayer::SeekForward;
      break;

    case model::RemoteCommand::SeekBackward:
      key = &keybinding::MediaPlayer::SeekBackward;
      break;

    case model::RemoteCommand::ToggleRepeat:
      key = &keybinding::MediaPlayer::ToggleRepeat;
      break;

    case model::RemoteCommand::ToggleShuffle:
      key = &keybinding::MediaPlayer::ToggleShuffle;
      break;

    case model::RemoteCommand::Quit:
      if (auto dispatcher = GetDispatcher(); dispatcher) dispatcher->SendEvent(CustomEvent::Exit());
      break;

    case model::RemoteCommand::SetVolume:
    case model::RemoteCommand::Seek:
      // There is nothing to do without a value
      break;
  }

  if (!key) return;

  // Reuse handlers from keyboard, to keep the same behavior (and UI state) for both of them
  if (HandleMediaEvent(*key) || HandleVolumeEvent(*key)) return;

  HandleSeekEvent(*key);
}

/* ********************************************************************************************** */

bool MediaPlayer::HandleRemoteValue(const model::RemoteRequest& request) {
  auto dispatcher = GetDispatcher();
  if (!dispatcher) return false;

  const auto* number = std::get_if<model::RemoteNumber>(&request.value);

  if (request.command == model::RemoteCommand::SetVolume && number) {
    const int current = static_cast<int>(std::round(volume_.GetLevel() * kMaxVolume));
    const int level = std::clamp(number->relative ? current + number->value : number->value, 0,
                                 static_cast<int>(kMaxVolume));

    if (level == current) return true;

    // Mute state does not depend on volume level, so it is kept
    model::Volume volume{static_cast<float>(level) / kMaxVolume};
    if (volume_.IsMuted()) volume.ToggleMute();

    volume_ = volume;
    dispatcher->SendEvent(interface::CustomEvent::SetAudioVolume(volume_));

    SaveVolume();
    return true;
  }

  if (request.command == model::RemoteCommand::Seek && number) {
    if (!IsPlaying()) return true;

    // Player ignores a position outside of song, so use the closest one instead
    const int position = static_cast<int>(song_.curr_info.position);
    const int last = std::max(static_cast<int>(song_.duration) - 1, 0);
    const int target =
        std::clamp(number->relative ? position + number->value : number->value, 0, last);

    if (target > position) {
      dispatcher->SendEvent(interface::CustomEvent::SeekForwardPosition(target - position));
    } else if (target < position) {
      dispatcher->SendEvent(interface::CustomEvent::SeekBackwardPosition(position - target));
    }

    return true;
  }

  if (const auto* mode = std::get_if<model::RepeatMode>(&request.value);
      request.command == model::RemoteCommand::ToggleRepeat && mode) {
    if (*mode == repeat_) return true;

    repeat_ = *mode;
    dispatcher->SendEvent(interface::CustomEvent::SetRepeatMode(repeat_));
    return true;
  }

  if (const auto* enabled = std::get_if<bool>(&request.value);
      request.command == model::RemoteCommand::ToggleShuffle && enabled) {
    if (*enabled == shuffle_) return true;

    shuffle_ = *enabled;
    dispatcher->SendEvent(interface::CustomEvent::SetShuffle(shuffle_));
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void MediaPlayer::SaveVolume() const {
  // Mute state is not saved, only the volume level
  const int level = static_cast<int>(std::round(volume_.GetLevel() * kMaxVolume));
  if (!file_handler_->SaveSettings(model::Settings{.volume = level})) ERROR("Cannot save volume");
}

}  // namespace interface
