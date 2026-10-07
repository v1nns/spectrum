#include "model/remote_command.h"

#include <algorithm>
#include <cctype>

namespace model {

namespace {

constexpr std::string_view kSpaces = " \t";  //!< Characters ignored around command and its value
constexpr char kTimeSeparator = ':';         //!< Between hours, minutes and seconds
constexpr char kIncrease = '+';              //!< Prefix for a number added to the current value
constexpr char kDecrease = '-';              //!< Prefix for a number subtracted from current value

constexpr std::string_view kEnabled = "on";    //!< Value to enable a setting
constexpr std::string_view kDisabled = "off";  //!< Value to disable a setting

constexpr int kMaxVolume = 100;           //!< Volume is a percentage
constexpr std::size_t kMaxDigits = 6;     //!< Avoid numbers that do not fit
constexpr int kSecondsPerUnit = 60;       //!< Seconds in a minute, and minutes in an hour
constexpr std::size_t kMaxTimeUnits = 3;  //!< Hours, minutes and seconds

/* ********************************************************************************************** */

//! Remove spaces around text
std::string_view Trim(std::string_view text) {
  const auto first = text.find_first_not_of(kSpaces);
  if (first == std::string_view::npos) return {};

  return text.substr(first, text.find_last_not_of(kSpaces) - first + 1);
}

/* ********************************************************************************************** */

//! Convert text containing only digits to a number
std::optional<int> ParseDigits(std::string_view text) {
  if (text.empty() || text.size() > kMaxDigits) return std::nullopt;

  int number = 0;

  for (char digit : text) {
    if (std::isdigit(static_cast<unsigned char>(digit)) == 0) return std::nullopt;
    number = number * 10 + (digit - '0');  // NOLINT: decimal base
  }

  return number;
}

/* ********************************************************************************************** */

//! Convert time (seconds, "minutes:seconds" or "hours:minutes:seconds") to seconds
std::optional<int> ParseTime(std::string_view text) {
  int seconds = 0;
  std::size_t units = 0;

  while (true) {
    const auto separator = text.find(kTimeSeparator);
    const auto unit = ParseDigits(text.substr(0, separator));

    // Only the first unit may be bigger than a minute (or hour), e.g. "90" or "90:00"
    if (!unit || ++units > kMaxTimeUnits || (units > 1 && *unit >= kSecondsPerUnit))
      return std::nullopt;

    seconds = seconds * kSecondsPerUnit + *unit;

    if (separator == std::string_view::npos) break;
    text.remove_prefix(separator + 1);
  }

  return seconds;
}

/* ********************************************************************************************** */

//! Convert text to a number (or to a change on current value, when it starts with a sign)
template <typename Parser>
std::optional<RemoteNumber> ParseNumber(std::string_view text, Parser parser) {
  if (text.empty()) return std::nullopt;

  const bool increase = text.front() == kIncrease;
  const bool decrease = text.front() == kDecrease;
  if (increase || decrease) text.remove_prefix(1);

  const std::optional<int> number = parser(text);
  if (!number) return std::nullopt;

  return RemoteNumber{.value = decrease ? -*number : *number, .relative = increase || decrease};
}

/* ********************************************************************************************** */

//! Create error message for a value that is not accepted by command
std::string InvalidValue(std::string_view name, std::string_view value, std::string_view expected) {
  std::string message =
      value.empty() ? "missing value" : "invalid value \"" + std::string{value} + "\"";

  return message + " for command \"" + std::string{name} + "\" (expected " + std::string{expected} +
         ")";
}

}  // namespace

/* ********************************************************************************************** */

bool operator==(const RemoteNumber& lhs, const RemoteNumber& rhs) {
  return lhs.value == rhs.value && lhs.relative == rhs.relative;
}

/* ********************************************************************************************** */

std::ostream& operator<<(std::ostream& out, const RemoteNumber& number) {
  if (number.relative && number.value >= 0) out << kIncrease;
  return out << number.value;
}

/* ********************************************************************************************** */

bool operator==(const RemoteRequest& lhs, const RemoteRequest& rhs) {
  return lhs.command == rhs.command && lhs.value == rhs.value;
}

/* ********************************************************************************************** */

bool operator!=(const RemoteRequest& lhs, const RemoteRequest& rhs) { return !(lhs == rhs); }

/* ********************************************************************************************** */

std::ostream& operator<<(std::ostream& out, const RemoteRequest& request) {
  out << request.command;

  if (const auto* number = std::get_if<RemoteNumber>(&request.value); number) out << " " << *number;
  if (const auto* mode = std::get_if<RepeatMode>(&request.value); mode) out << " " << *mode;
  if (const auto* flag = std::get_if<bool>(&request.value); flag)
    out << " " << (*flag ? kEnabled : kDisabled);
  if (const auto* text = std::get_if<std::string>(&request.value); text) out << " " << *text;

  return out;
}

/* ********************************************************************************************** */

std::optional<RemoteRequest> ParseRemoteRequest(std::string_view text, std::string& error) {
  text = Trim(text);

  // Everything after command name is its value (which may contain spaces, like a file path)
  const auto name_end = text.find_first_of(kSpaces);
  const std::string_view name = text.substr(0, name_end);
  const std::string_view value =
      name_end != std::string_view::npos ? Trim(text.substr(name_end)) : std::string_view{};

  const auto command = ParseRemoteCommand(name);

  if (!command) {
    error = "unknown command \"" + std::string{name} + "\"";
    return std::nullopt;
  }

  switch (*command) {
    case RemoteCommand::SetVolume: {
      const auto number = ParseNumber(value, ParseDigits);

      if (!number || (!number->relative && number->value > kMaxVolume)) {
        error = InvalidValue(name, value, "a number from 0 to 100, or a change like +5 or -5");
        return std::nullopt;
      }

      return RemoteRequest{*command, *number};
    }

    case RemoteCommand::Seek: {
      const auto number = ParseNumber(value, ParseTime);

      if (!number) {
        error =
            InvalidValue(name, value, "a position like 90 or 1:30, or a change like +10 or -10");
        return std::nullopt;
      }

      return RemoteRequest{*command, *number};
    }

    case RemoteCommand::ToggleRepeat: {
      if (value.empty()) return RemoteRequest{*command};

      for (auto mode : {RepeatMode::Off, RepeatMode::All, RepeatMode::One}) {
        if (value == GetRepeatModeName(mode)) return RemoteRequest{*command, mode};
      }

      error = InvalidValue(name, value, "off, all or one");
      return std::nullopt;
    }

    case RemoteCommand::ToggleShuffle: {
      if (value.empty()) return RemoteRequest{*command};

      if (value == kEnabled || value == kDisabled)
        return RemoteRequest{*command, value == kEnabled};

      error = InvalidValue(name, value, "on or off");
      return std::nullopt;
    }

    case RemoteCommand::Play:
      // File, directory, URL or name of playlist to play
      if (!value.empty()) return RemoteRequest{*command, std::string{value}};
      break;

    default:
      if (!value.empty()) {
        error = "command \"" + std::string{name} + "\" does not accept a value";
        return std::nullopt;
      }
      break;
  }

  return RemoteRequest{*command};
}

}  // namespace model
