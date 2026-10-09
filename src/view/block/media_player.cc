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

    // Knob goes from the first column (no progress) to the last one (full), and everything
    // before it is filled. Without a knob, all columns are filled only with full progress
    const int knob = static_cast<int>(std::round(progress_ * static_cast<float>(width - 1)));
    const int filled =
        show_knob_ ? knob : static_cast<int>(std::round(progress_ * static_cast<float>(width)));

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
    // Knob follows mouse while song position is being picked with it
    // (and it stays there until player informs the new position)
    const uint32_t current =
        seek_drag_.value_or(seek_pending_.value_or(song_.curr_info.position));

    position = (float)current / (float)song_.duration;
    curr_time = model::time_to_string(current);
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
  auto mode = [&theme](const std::string& text, bool enabled, bool hovered) {
    const auto& colors = theme.mode_enabled;
    ftxui::Element content = ftxui::text(" " + text + " ");

    // With mouse over it, mode uses the same colors from anything else hovered in this block
    if (enabled) {
      return content | ftxui::color(colors.foreground) |
             ftxui::bgcolor(hovered ? theme.duration_focused.foreground : colors.background);
    }

    return content | ftxui::dim | (hovered ? ftxui::bgcolor(theme.button_hovered) : ftxui::nothing);
  };

  ftxui::Element modes = ftxui::hbox({
      mode(std::string{"shuffle "} + (shuffle_ ? "on" : "off"), shuffle_, is_shuffle_hovered_) |
          ftxui::reflect(shuffle_box_),
      mode("repeat " + std::string{model::GetRepeatModeName(repeat_)},
           repeat_ != model::RepeatMode::Off, is_repeat_hovered_) |
          ftxui::reflect(repeat_box_),
  });

  // Current volume, as a line and as a percentage
  std::ostringstream ss;
  ss << std::setfill(' ') << std::setw(4) << ((int)volume_) << "% ";

  ftxui::Element volume = ftxui::hbox({
      ftxui::text("vol ") |
          (is_volume_hovered_ ? ftxui::color(theme.duration_focused.foreground) | ftxui::bold
                              : ftxui::dim) |
          ftxui::reflect(volume_label_box_),
      progress_line(static_cast<float>(volume_),
                    is_volume_hovered_ ? theme.duration_focused : theme.duration) |
          ftxui::size(WIDTH, EQUAL, kVolumeColumns) | ftxui::reflect(volume_line_box_),
      ftxui::text(std::move(ss).str()) | ftxui::color(theme.text),
  });

  // With mouse over it, volume is not dimmed (otherwise, it would look the same while muted)
  if (volume_.IsMuted()) {
    volume = volume | (is_volume_hovered_ ? ftxui::nothing : ftxui::dim) |
             ftxui::color(theme.volume_muted);
  }

  volume = volume | ftxui::reflect(volume_box_);

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

  return RenderWindow(RenderTitle(" player "), content | ftxui::size(HEIGHT, EQUAL, kMaxRows));
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
    seek_drag_.reset();
    seek_pending_.reset();
    btn_play_->ResetState();
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");
    song_ = event.GetContent<model::Song>();
    seek_pending_.reset();
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongState) {
    song_.curr_info = event.GetContent<model::Song::CurrentInformation>();

    // Stop showing the position asked with mouse, as player is already on it (or it gave up)
    if (seek_pending_.has_value()) {
      const int difference = std::abs(static_cast<int>(song_.curr_info.position) -
                                      static_cast<int>(*seek_pending_));

      if (difference <= kSeekTolerance || ++seek_pending_updates_ >= kMaxSeekPendingUpdates ||
          !IsPlaying()) {
        seek_pending_.reset();
      }
    }

    if (song_.curr_info.state == model::Song::MediaState::Play) btn_play_->SetState(true);
  }

  return false;
}

/* ********************************************************************************************** */

bool MediaPlayer::OnMouseEvent(ftxui::Event event) {
  // While song position is being picked, nothing else handles mouse (e.g. button released over
  // a media button must not click on it)
  if (seek_drag_.has_value() && HandleSeekMouseEvent(event)) return true;

  if (OnTitleMouseEvent(event)) return true;

  // Mouse focus on shuffle mode, repeat mode and volume
  is_shuffle_hovered_ = shuffle_box_.Contain(event.mouse().x, event.mouse().y);
  is_repeat_hovered_ = repeat_box_.Contain(event.mouse().x, event.mouse().y);
  is_volume_hovered_ = volume_box_.Contain(event.mouse().x, event.mouse().y);

  // Media buttons
  if (btn_previous_->OnMouseEvent(event)) return true;
  if (btn_play_->OnMouseEvent(event)) return true;
  if (btn_stop_->OnMouseEvent(event)) return true;
  if (btn_next_->OnMouseEvent(event)) return true;

  if (HandleModeMouseEvent(event)) return true;

  if (HandleVolumeMouseEvent(event)) return true;

  return HandleSeekMouseEvent(event);
}

/* ********************************************************************************************** */

uint32_t MediaPlayer::GetSongPositionAt(int column) const {
  // The first column from line is the beginning of song and the last one is its end (exactly like
  // knob is drawn)
  const int real_x = std::clamp(column, duration_box_.x_min, duration_box_.x_max) -
                     duration_box_.x_min;
  const int last_x = std::max(1, duration_box_.x_max - duration_box_.x_min);

  return static_cast<uint32_t>(
      std::lround(static_cast<double>(song_.duration) * real_x / static_cast<double>(last_x)));
}

/* ********************************************************************************************** */

bool MediaPlayer::HandleSeekMouseEvent(ftxui::Event& event) {
  const auto& mouse = event.mouse();
  const bool dragging = seek_drag_.has_value();

  seek_drag_.reset();

  if (!IsPlaying()) return false;

  const bool on_line = duration_box_.Contain(mouse.x, mouse.y);

  // Mouse focus on song duration box
  is_duration_focused_ = on_line;

  if (mouse.button != ftxui::Mouse::Left) return false;

  // While button is held, knob follows mouse along the line (even past both ends of it), and it
  // goes back to song position if mouse leaves this line
  if (mouse.motion != ftxui::Mouse::Released) {
    if (!on_line && !(dragging && mouse.y == duration_box_.y_min)) return false;

    seek_drag_ = GetSongPositionAt(mouse.x);
    is_duration_focused_ = true;
    return true;
  }

  if (!on_line && !dragging) return false;

  // Position is changed only when button is released, otherwise song would be moved more than
  // once by an offset based on a position not updated yet
  const auto new_position = static_cast<int>(GetSongPositionAt(mouse.x));
  const auto position = static_cast<int>(song_.curr_info.position);

  // Do nothing if result is equal the current position
  if (new_position == position) return true;

  LOG("Handle left click mouse event on song progress bar");
  auto dispatcher = GetDispatcher();

  // Send event to player
  const int offset = std::abs(new_position - position);

  interface::CustomEvent event_seek = new_position > position
                                          ? interface::CustomEvent::SeekForwardPosition(offset)
                                          : interface::CustomEvent::SeekBackwardPosition(offset);

  LOG("Sending event to ", event_seek.GetId(), " with offset=", offset);
  dispatcher->SendEvent(event_seek);

  // Keep knob on this position until player informs it (unless it is the end of song, which is
  // ignored by player)
  if (static_cast<uint32_t>(new_position) < song_.duration) {
    seek_pending_ = static_cast<uint32_t>(new_position);
    seek_pending_updates_ = 0;
  }

  // Set this block as active (focused)
  auto event_focus = interface::CustomEvent::SetFocused(GetId());
  dispatcher->SendEvent(event_focus);

  return true;
}

/* ********************************************************************************************** */

bool MediaPlayer::HandleModeMouseEvent(ftxui::Event& event) {
  const auto& mouse = event.mouse();

  if (mouse.button != ftxui::Mouse::Left || mouse.motion != ftxui::Mouse::Released) return false;

  const keybinding::Key* key = nullptr;

  if (shuffle_box_.Contain(mouse.x, mouse.y)) key = &keybinding::MediaPlayer::ToggleShuffle;
  if (repeat_box_.Contain(mouse.x, mouse.y)) key = &keybinding::MediaPlayer::ToggleRepeat;

  if (!key) return false;

  LOG("Handle left click mouse event on shuffle/repeat mode");
  AskForFocus();

  // Reuse handler from keyboard, to keep the same behavior for both of them
  return HandleMediaEvent(*key);
}

/* ********************************************************************************************** */

bool MediaPlayer::HandleVolumeMouseEvent(ftxui::Event& event) {
  const auto& mouse = event.mouse();

  if (!volume_box_.Contain(mouse.x, mouse.y)) return false;

  // Mouse wheel changes volume by the same step used by its keys
  if (mouse.button == ftxui::Mouse::WheelUp || mouse.button == ftxui::Mouse::WheelDown) {
    LOG("Handle mouse wheel event on volume");
    AskForFocus();

    HandleVolumeEvent(mouse.button == ftxui::Mouse::WheelUp ? keybinding::MediaPlayer::VolumeUp
                                                            : keybinding::MediaPlayer::VolumeDown);
    return true;
  }

  if (mouse.button != ftxui::Mouse::Left || mouse.motion != ftxui::Mouse::Released) return false;

  // A click on label mutes volume (or restores it), as line cannot be clicked before its start
  if (volume_label_box_.Contain(mouse.x, mouse.y)) {
    LOG("Handle left click mouse event on volume label");
    AskForFocus();

    HandleVolumeEvent(keybinding::MediaPlayer::Mute);
    return true;
  }

  if (!volume_line_box_.Contain(mouse.x, mouse.y)) return false;

  LOG("Handle left click mouse event on volume line");
  AskForFocus();

  // Line is filled up to the column clicked (which is the last one for maximum volume)
  const int columns = volume_line_box_.x_max - volume_line_box_.x_min + 1;
  const int clicked = mouse.x - volume_line_box_.x_min + 1;
  const int level = static_cast<int>(std::lround(kMaxVolume * static_cast<float>(clicked) /
                                                 static_cast<float>(columns)));

  // Reuse handler from remote command, as it also sets volume to a given level
  HandleRemoteValue(model::RemoteRequest{model::RemoteCommand::SetVolume,
                                         model::RemoteNumber{.value = level, .relative = false}});
  return true;
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

    auto event_seek = interface::CustomEvent::SeekForwardPosition(kSeekSeconds);
    dispatcher->SendEvent(event_seek);

    return true;
  }

  // Seek backward in current song
  if (event == keybinding::MediaPlayer::SeekBackward && IsPlaying()) {
    LOG("Handle key to seek backward in current song");
    auto dispatcher = GetDispatcher();

    auto event_seek = interface::CustomEvent::SeekBackwardPosition(kSeekSeconds);
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
