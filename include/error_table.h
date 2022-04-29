/**
 * \file
 * \brief  All error codes from application in a single map
 */

#ifndef INCLUDE_ERROR_TABLE_H_
#define INCLUDE_ERROR_TABLE_H_

#include <algorithm>
#include <array>
#include <cassert>
#include <string_view>

namespace error {

//! To make life easier in the first versions, error is simple an int
using Value = int;

//! Everything fine!
static constexpr Value kSuccess = 0;

//! Terminal errors
static constexpr Value kTerminalInitialization = 1;
static constexpr Value kTerminalColorsUnavailable = 2;

//! Song errors
static constexpr Value kInvalidFile = 30;
static constexpr Value kFileNotSupported = 31;
static constexpr Value kFileCompressionNotSupported = 32;
static constexpr Value kUnknownNumOfChannels = 33;
static constexpr Value kInconsistentHeaderInfo = 34;

/* ********************************************************************************************** */

/**
 * @brief Class holding the map with all possible errors that may occur during application lifetime
 */
class Table {
 private:
  //! Single entry for error message <code, message>
  using Message = std::pair<Value, std::string_view>;

  //! Array similar to a map and contains all "mapped" errors (pun intended)
  static constexpr std::array<Message, 6> kErrorMap{
      {{kTerminalInitialization, "Could not initialize screen"},
       {kTerminalColorsUnavailable, "No support to change colors"},
       {kInvalidFile, "Invalid file"},
       {kFileNotSupported, "File not supported"},
       {kFileCompressionNotSupported, "Decoding compressed file is not supported"},
       {kUnknownNumOfChannels,
        "File does not seem to be neither mono nor stereo (perhaps multi-track or corrupted)"},
       {kInconsistentHeaderInfo, "Header data is inconsistent"}},
  };

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Get the error associated to the specific code
   *
   * @param code Error code
   * @return Message Error detail
   */
  const std::string_view GetMessage(Value id) {
    auto find_error = [&id](Message element) { return element.first == id; };

    auto error = std::find_if(kErrorMap.begin(), kErrorMap.end(), find_error);
    assert(error != kErrorMap.end());

    return error->second;
  }
};

}  // namespace error
#endif  // INCLUDE_ERROR_TABLE_H_