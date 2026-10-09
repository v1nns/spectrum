/**
 * \file
 * \brief  Class for block containing audio player
 */

#ifndef INCLUDE_VIEW_BLOCK_AUDIO_PLAYER_H_
#define INCLUDE_VIEW_BLOCK_AUDIO_PLAYER_H_

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "ftxui/dom/elements.hpp"
#include "model/remote_command.h"
#include "model/repeat_mode.h"
#include "model/song.h"
#include "model/volume.h"
#include "util/file_handler.h"
#include "view/base/block.h"
#include "view/element/button.h"
#include "view/element/flash_message.h"

namespace interface {

/**
 * @brief Component with detailed information about the chosen file (in this case, some music file)
 */
class MediaPlayer : public Block {
  static constexpr int kMaxRows = 4;         //!< Maximum rows for the Component
  static constexpr int kMarginColumns = 2;   //!< Empty columns on both sides of content
  static constexpr int kSeekSeconds = 5;     //!< Seconds to seek forward or backward by a key
  static constexpr int kVolumeColumns = 10;  //!< Columns for line with volume level

  //! Empty columns between label and line with volume level, where a click silences volume (as
  //! line is filled up to the column clicked, there is no column in it for that)
  static constexpr int kVolumeZeroColumns = 1;

  //! Seconds that position informed by player may differ from the one asked with mouse
  static constexpr int kSeekTolerance = 1;

  //! Updates from player to wait for the position asked with mouse (it may never come, as player
  //! ignores a position outside of song)
  static constexpr int kMaxSeekPendingUpdates = 2;

  //! Time that a warning stays visible
  static constexpr std::chrono::milliseconds kWarningDuration{4000};

 public:
  /**
   * @brief Construct a new Audio Player object
   * @param dispatcher Block event dispatcher
   * @param file_handler Utility handler to load/save volume (if null, a new one is created)
   */
  explicit MediaPlayer(const std::shared_ptr<EventDispatcher>& dispatcher,
                       const std::shared_ptr<util::FileHandler>& file_handler = nullptr);

  /**
   * @brief Destroy the Audio Player object
   */
  ~MediaPlayer() override = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(ftxui::Event event) override;

  /**
   * @brief Handles a custom event
   *
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
 private:
  //! Handle mouse event
  bool OnMouseEvent(ftxui::Event event);

  /**
   * @brief Handle mouse event on shuffle and repeat modes
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool HandleModeMouseEvent(ftxui::Event& event);

  /**
   * @brief Handle mouse event on volume
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool HandleVolumeMouseEvent(ftxui::Event& event);

  /**
   * @brief Handle mouse event on line with song duration: its knob follows mouse while button is
   * held, and song position is changed when it is released
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool HandleSeekMouseEvent(ftxui::Event& event);

  //! Get song position (in seconds) for the given column from screen, which is limited to both
  //! ends of line with song duration
  uint32_t GetSongPositionAt(int column) const;

  /**
   * @brief Handle event for media control (e.g., play/pause, stop, clear and skip song)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool HandleMediaEvent(const ftxui::Event& event);

  /**
   * @brief Handle event for volume control
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool HandleVolumeEvent(const ftxui::Event& event);

  //! Save current volume level, so it is restored on next run
  void SaveVolume() const;

  /**
   * @brief Handle event for seek position in song
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool HandleSeekEvent(const ftxui::Event& event) const;

  /**
   * @brief Execute command sent from command-line, exactly like its key was pressed (or using the
   * value given to it, which is something that a key cannot do)
   * @param request Remote command and its value
   */
  void HandleRemoteCommand(const model::RemoteRequest& request);

  /**
   * @brief Execute command sent from command-line with a value
   * @param request Remote command and its value
   * @return true if value was used, otherwise false (command must be executed like a key press)
   */
  bool HandleRemoteValue(const model::RemoteRequest& request);

  //! Create event to skip song (handled by audio player, as songs are always played from a queue)
  static CustomEvent CreateSkipEvent(bool next);

  //! Get title to show for current song (its source is used when song does not have one)
  std::string GetSongTitle() const;

  //! Utility to check media state
  bool IsPlaying() const {
    return song_.curr_info.state == model::Song::MediaState::Play ||
           song_.curr_info.state == model::Song::MediaState::Pause;
  }

  /* ******************************************************************************************** */
  //! Variables

  MediaButton btn_play_;      //!< Media player button for Play/Pause
  MediaButton btn_stop_;      //!< Media player button for Stop
  MediaButton btn_previous_;  //!< Media player button for Play next song
  MediaButton btn_next_;      //!< Media player button for Play previous song

  model::Song song_ = model::Song{};  //!< Audio information from current song
  model::Volume volume_;              //!< General sound volume

  model::RepeatMode repeat_ = model::RepeatMode::Off;  //!< Repeat mode for songs from queue
  bool shuffle_ = false;                               //!< Shuffle songs from queue

  ftxui::Box shuffle_box_;      //!< Box for shuffle mode
  ftxui::Box repeat_box_;       //!< Box for repeat mode
  ftxui::Box volume_box_;       //!< Box for volume (label, line and percentage)
  ftxui::Box volume_label_box_; //!< Box for label from volume
  ftxui::Box volume_line_box_;  //!< Box for line with volume level

  bool is_shuffle_hovered_ = false;  //!< Flag to control if mouse cursor is over shuffle mode
  bool is_repeat_hovered_ = false;   //!< Flag to control if mouse cursor is over repeat mode
  bool is_volume_hovered_ = false;   //!< Flag to control if mouse cursor is over volume

  ftxui::Box duration_box_;           //!< Box for song duration component (line)
  bool is_duration_focused_ = false;  //!< Flag to control if song duration box is focused

  //! Song position (in seconds) picked with mouse, while its button is still held
  std::optional<uint32_t> seek_drag_;

  //! Song position (in seconds) asked to player with mouse, which is shown until player informs
  //! it (otherwise knob would go back to the old position for a moment)
  std::optional<uint32_t> seek_pending_;
  int seek_pending_updates_ = 0;  //!< Updates from player without the position asked

  FlashMessage warning_;  //!< Brief warning shown above song duration (e.g. file not supported)

  std::shared_ptr<util::FileHandler> file_handler_;  //!< Load/save volume
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_AUDIO_PLAYER_H_
