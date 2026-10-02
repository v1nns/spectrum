/**
 * \file
 * \brief  All error codes from application in a single map
 */

#ifndef INCLUDE_MODEL_APPLICATION_ERROR_H_
#define INCLUDE_MODEL_APPLICATION_ERROR_H_

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <string_view>

namespace error {

//! To make life easier in the first versions, error is simple an int
using Code = int;

//! Everything fine!
static constexpr Code kSuccess = 0;
static constexpr Code kUnknownError = 99;

//! Terminal errors
static constexpr Code kTerminalInitialization = 1;
static constexpr Code kTerminalColorsUnavailable = 2;

//! File and directory navigation
static constexpr Code kAccessDirFailed = 20;

//! Song errors
static constexpr Code kInvalidFile = 30;
static constexpr Code kFileNotSupported = 31;
static constexpr Code kFileCompressionNotSupported = 32;
static constexpr Code kUnknownNumOfChannels = 33;
static constexpr Code kInconsistentHeaderInfo = 34;
static constexpr Code kCorruptedData = 35;

//! ALSA driver errors
static constexpr Code kSetupAudioParamsFailed = 50;
static constexpr Code kPlaybackFailed = 51;

//! FFMPEG driver errors
static constexpr Code kDecodeFileFailed = 70;
static constexpr Code kSeekFrameFailed = 71;
static constexpr Code kEqualizerFailed = 72;

//! Playlist errors
static constexpr Code kTooManyFailedSongs = 80;

//! Streaming errors
static constexpr Code kStreamFetchFailed = 90;

//! How error is presented to user
enum class Level : std::uint8_t {
  Critical,  //!< Shown in a dialog, user must close it to continue
  Warning,   //!< Shown briefly, without interrupting user
};

/* ********************************************************************************************** */

/**
 * @brief Class holding the map with all possible errors that may occur during application lifetime
 */
class ApplicationError {
 private:
  //! Single entry for error
  struct Message {
    Code code;                 //!< Error code
    Level level;               //!< How error is presented to user
    std::string_view message;  //!< Error message
  };

  //! Array similar to a map and contains all "mapped" errors (pun intended)
  static constexpr std::array<Message, 17> kErrorMap{{
      {kTerminalInitialization, Level::Critical, "Cannot initialize screen"},
      {kTerminalColorsUnavailable, Level::Critical, "No support to change colors"},
      {kAccessDirFailed, Level::Warning, "Cannot access directory"},
      {kInvalidFile, Level::Warning, "Invalid file"},
      {kFileNotSupported, Level::Warning, "File not supported"},
      {kFileCompressionNotSupported, Level::Warning, "Decoding compressed file is not supported"},
      {kUnknownNumOfChannels, Level::Warning,
       "File does not seem to be neither mono nor stereo (perhaps multi-track or corrupted)"},
      {kInconsistentHeaderInfo, Level::Warning, "Header data is inconsistent"},
      {kCorruptedData, Level::Warning, "File is corrupted"},
      {kSetupAudioParamsFailed, Level::Critical, "Cannot set audio parameters"},
      {kPlaybackFailed, Level::Critical,
       "Cannot play audio on output device (was it disconnected?)"},
      {kDecodeFileFailed, Level::Warning, "Cannot decode song"},
      {kSeekFrameFailed, Level::Warning, "Cannot seek frame in song"},
      {kEqualizerFailed, Level::Warning, "Cannot apply equalizer settings"},
      {kTooManyFailedSongs, Level::Critical, "Several songs failed in a row, playlist was stopped"},
      {kStreamFetchFailed, Level::Warning, "Cannot fetch song from URL"},
      {kUnknownError, Level::Critical,
       "Unknown error used for almost everything during development =)"},
  }};

  //! Find entry for the given error code
  static const Message& Find(Code id) {
    auto find_error = [&id](const Message& element) { return element.code == id; };

    auto error = std::find_if(kErrorMap.begin(), kErrorMap.end(), find_error);
    assert(error != kErrorMap.end());

    return *error;
  }

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Get the error associated to the specific code
   *
   * @param code Error code
   * @return Message Error detail
   */
  static std::string_view GetMessage(Code id) { return Find(id).message; }

  /**
   * @brief Get how error must be presented to user
   *
   * @param id Error code
   * @return Level Error level
   */
  static Level GetLevel(Code id) { return Find(id).level; }
};

}  // namespace error
#endif  // INCLUDE_MODEL_APPLICATION_ERROR_H_