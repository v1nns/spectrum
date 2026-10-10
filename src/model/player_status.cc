#include "model/player_status.h"

#include <cmath>
#include <string_view>

#include "nlohmann/json.hpp"

namespace model {

namespace {

//! Field names used in JSON object (and also to format it as text)
constexpr char kFieldState[] = "state";
constexpr char kFieldArtist[] = "artist";
constexpr char kFieldTitle[] = "title";
constexpr char kFieldPosition[] = "position";
constexpr char kFieldDuration[] = "duration";
constexpr char kFieldVolume[] = "volume";
constexpr char kFieldMuted[] = "muted";
constexpr char kFieldRepeat[] = "repeat";
constexpr char kFieldShuffle[] = "shuffle";
constexpr char kFieldOutputDevice[] = "output_device";
constexpr char kFieldOutputSampleRate[] = "output_sample_rate";
constexpr char kFieldOutputBitDepth[] = "output_bit_depth";

//! Delimiters for a field name in the text to format
constexpr char kFieldBegin = '{';
constexpr char kFieldEnd = '}';

constexpr int kNoIndentation = -1;   //!< Write the whole JSON object in a single line
constexpr float kMaxVolume = 100.F;  //!< Volume is sent as percentage

/* ********************************************************************************************** */

//! Get name for song state (any state other than playing or paused means there is nothing to play)
std::string_view GetStateName(Song::MediaState state) {
  switch (state) {
    case Song::MediaState::Play:
      return "playing";
    case Song::MediaState::Pause:
      return "paused";
    default:
      break;
  }

  return "stopped";
}

/* ********************************************************************************************** */

//! Get field value as text
std::string GetText(const std::string& name, const nlohmann::json& value) {
  if (value.is_string()) return value.get<std::string>();
  if (value.is_boolean()) return value.get<bool>() ? "on" : "off";

  // Same format used by media player
  if (value.is_number_unsigned() && (name == kFieldPosition || name == kFieldDuration))
    return time_to_string(value.get<uint32_t>());

  return value.dump();
}

}  // namespace

/* ********************************************************************************************** */

std::string to_json(const PlayerStatus& status) {
  nlohmann::json json;

  json[kFieldState] = GetStateName(status.state);
  json[kFieldArtist] = status.artist;
  json[kFieldTitle] = status.title;
  json[kFieldPosition] = status.position;
  json[kFieldDuration] = status.duration;
  json[kFieldVolume] = static_cast<int>(std::round(status.volume.GetLevel() * kMaxVolume));
  json[kFieldMuted] = status.volume.IsMuted();
  json[kFieldRepeat] = GetRepeatModeName(status.repeat);
  json[kFieldShuffle] = status.shuffle;

  // Without a song, there is no output in use (so these fields are empty, as the ones from song)
  json[kFieldOutputDevice] = status.output ? status.output->device : "";
  json[kFieldOutputSampleRate] = status.output ? status.output->format.sample_rate : 0U;
  json[kFieldOutputBitDepth] = status.output ? status.output->format.GetBitDepth() : 0U;

  // Metadata from song may contain invalid text, which is replaced instead of throwing an error
  return json.dump(kNoIndentation, ' ', /*ensure_ascii=*/false,
                   nlohmann::json::error_handler_t::replace);
}

/* ********************************************************************************************** */

std::optional<std::string> format_status(const std::string& json,
                                         const std::optional<std::string>& text_format) {
  const nlohmann::json status = nlohmann::json::parse(json, nullptr, /*allow_exceptions=*/false);

  if (!status.is_object()) return std::nullopt;
  if (!text_format) return json;

  const std::string& format = *text_format;

  std::string text;
  std::size_t current = 0;

  while (current < format.size()) {
    const std::size_t begin = format.find(kFieldBegin, current);
    const std::size_t end =
        begin != std::string::npos ? format.find(kFieldEnd, begin) : std::string::npos;

    // No more fields to replace
    if (end == std::string::npos) break;

    const std::string name = format.substr(begin + 1, end - begin - 1);

    if (auto field = status.find(name); field != status.end()) {
      text += format.substr(current, begin - current) + GetText(name, *field);
      current = end + 1;
    } else {
      // Not a field, so keep it (and check again from the next character, e.g. "{{title}")
      text += format.substr(current, begin + 1 - current);
      current = begin + 1;
    }
  }

  return text + format.substr(current);
}

}  // namespace model
