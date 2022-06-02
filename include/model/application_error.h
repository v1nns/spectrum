/**
 * \file
 * \brief  All error codes from application in a single map
 */

#ifndef INCLUDE_MODEL_APPLICATION_ERROR_H_
#define INCLUDE_MODEL_APPLICATION_ERROR_H_

#include <algorithm>
#include <array>
#include <cassert>
#include <string_view>

namespace error {

//! To make life easier in the first versions, error is simple an int
// TODO: next step is to add a level (like critical or non-critical, warning, ...)
using Code = int;

//! Everything fine!
static constexpr Code kSuccess = 0;
static constexpr Code kUnknownError = 99;

//! Terminal errors
static constexpr Code kTerminalInitialization = 1;
static constexpr Code kTerminalColorsUnavailable = 2;

//! Song errors
static constexpr Code kInvalidFile = 30;
static constexpr Code kFileNotSupported = 31;
static constexpr Code kFileCompressionNotSupported = 32;
static constexpr Code kUnknownNumOfChannels = 33;
static constexpr Code kInconsistentHeaderInfo = 34;
static constexpr Code kCorruptedData = 35;

//! Driver errors
static constexpr Code kSetupAudioParamsFailed = 50;

/* ********************************************************************************************** */

/**
 * @brief Class holding the map with all possible errors that may occur during application lifetime
 */
class ApplicationError {
 private:
  //! Single entry for error message <code, message>
  using Message = std::pair<Code, std::string_view>;

  //! Array similar to a map and contains all "mapped" errors (pun intended)
  static constexpr std::array<Message, 10> kErrorMap{{
      {kTerminalInitialization, "Could not initialize screen"},
      {kTerminalColorsUnavailable, "No support to change colors"},
      {kInvalidFile, "Invalid file"},
      {kFileNotSupported, "File not supported"},
      {kFileCompressionNotSupported, "Decoding compressed file is not supported"},
      {kUnknownNumOfChannels,
       "File does not seem to be neither mono nor stereo (perhaps multi-track or corrupted)"},
      {kInconsistentHeaderInfo, "Header data is inconsistent"},
      {kCorruptedData, "File is corrupted"},
      {kSetupAudioParamsFailed, "Could not set audio parameters"},
      {kUnknownError, "Unknown error used for almost everything during development =)"},
  }};

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Get the error associated to the specific code
   *
   * @param code Error code
   * @return Message Error detail
   */
  static const std::string_view GetMessage(Code id) {
    auto find_error = [&id](Message element) { return element.first == id; };

    auto error = std::find_if(kErrorMap.begin(), kErrorMap.end(), find_error);
    assert(error != kErrorMap.end());

    return error->second;
  }
};

}  // namespace error
#endif  // INCLUDE_MODEL_APPLICATION_ERROR_H_