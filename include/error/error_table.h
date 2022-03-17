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

//! Everything fine!
static constexpr int kSuccess = 0;

//! Terminal errors
static constexpr int kTerminalInitialization = 1;
static constexpr int kTerminalColorsUnavailable = 2;

/* ********************************************************************************************** */

//! Single entry for error message <code, message>
using message_t = std::pair<int, std::string_view>;

/**
 * @brief Class containing a map with all possible errors to occur during application lifetime
 */
class ErrorTable {
 private:
  //! Map containing all "mapped" errors (pun intended)
  static constexpr std::array<message_t, 2> kErrorMap{
      {{kTerminalInitialization, "Could not initialize screen"},
       {kTerminalColorsUnavailable, "No support to change colors"}},
  };

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Get the error associated to the specific code
   *
   * @param code Error code
   * @return message_t Error detail
   */
  message_t GetMessage(int code) {
    auto find_error = [&code](message_t element) { return element.first == code; };

    auto error = std::find_if(kErrorMap.begin(), kErrorMap.end(), find_error);
    assert(error != kErrorMap.end());

    return *error;
  }
};

}  // namespace error
#endif  // INCLUDE_ERROR_TABLE_H_