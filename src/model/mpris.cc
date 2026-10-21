#include "model/mpris.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace model {

namespace {

//! Prefix for track identifier (paths starting with "/org/mpris" are reserved by specification)
constexpr std::string_view kTrackIdPrefix{"/org/spectrum/track/"};

constexpr std::string_view kFileScheme{"file://"};  //!< URI for a local file
constexpr std::string_view kHttpScheme{"http://"};
constexpr std::string_view kHttpsScheme{"https://"};

constexpr char kPathSeparator = '/';  //!< Path from local file must be absolute

//! Characters kept as they are in URL for a local file (anything else is encoded)
constexpr std::string_view kUnreservedSymbols{"-._~/"};
constexpr std::string_view kHexDigits{"0123456789ABCDEF"};

//! First bytes from a PNG image
constexpr std::string_view kPngSignature{"\x89PNG"};
constexpr char kEscape = '%';        //!< Prefix for a character encoded in URI
constexpr int kHexBase = 16;         //!< Encoded character is written as two hexadecimal digits
constexpr int kHexLetterValue = 10;  //!< Value of the first letter used as hexadecimal digit

constexpr double kMicrosecondsPerSecond = 1e6;  //!< MPRIS uses microseconds for time
constexpr double kMaxVolume = 100.;             //!< Volume is sent as percentage to player

//! Position moves forward by at most this many seconds between two updates, while playing
constexpr uint32_t kMaxPositionStep = 2;

/* ********************************************************************************************** */

//! Check if text starts with prefix
bool StartsWith(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}

/* ********************************************************************************************** */

//! Get value from hexadecimal digit (or nothing, if it is not one)
std::optional<int> ParseHexDigit(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return digit - 'a' + kHexLetterValue;
  if (digit >= 'A' && digit <= 'F') return digit - 'A' + kHexLetterValue;

  return std::nullopt;
}

/* ********************************************************************************************** */

//! Decode characters written as "%XX" (or nothing, if any of them is incomplete)
std::optional<std::string> DecodeUri(std::string_view text) {
  std::string decoded;

  for (std::size_t index = 0; index < text.size(); index++) {
    if (text[index] != kEscape) {
      decoded += text[index];
      continue;
    }

    if (index + 2 >= text.size()) return std::nullopt;

    auto high = ParseHexDigit(text[index + 1]);
    auto low = ParseHexDigit(text[index + 2]);
    if (!high || !low) return std::nullopt;

    decoded += static_cast<char>(*high * kHexBase + *low);
    index += 2;
  }

  return decoded;
}

/* ********************************************************************************************** */

//! Encode characters other than letters, digits and a few symbols as "%XX"
std::string EncodeUri(std::string_view text) {
  std::string encoded;

  for (char character : text) {
    const auto byte = static_cast<unsigned char>(character);

    if (std::isalnum(byte) != 0 || kUnreservedSymbols.find(character) != std::string_view::npos) {
      encoded += character;
      continue;
    }

    encoded += kEscape;
    encoded += kHexDigits[byte / kHexBase];
    encoded += kHexDigits[byte % kHexBase];
  }

  return encoded;
}

/* ********************************************************************************************** */

//! Check if song is loaded (only then position may be changed)
bool HasSong(const PlayerStatus& status) {
  return status.state == Song::MediaState::Play || status.state == Song::MediaState::Pause;
}

/* ********************************************************************************************** */

//! Convert time from MPRIS to seconds (limited to the values accepted by remote command)
int ToSeconds(int64_t microseconds) {
  const double seconds = std::round(static_cast<double>(microseconds) / kMicrosecondsPerSecond);
  const auto limit = static_cast<double>(std::numeric_limits<int>::max());

  return static_cast<int>(std::clamp(seconds, -limit, limit));
}

}  // namespace

/* ********************************************************************************************** */

std::string_view GetMprisPlaybackStatus(Song::MediaState state) {
  switch (state) {
    case Song::MediaState::Play:
      return "Playing";
    case Song::MediaState::Pause:
      return "Paused";
    default:
      break;
  }

  return "Stopped";
}

/* ********************************************************************************************** */

std::string_view GetMprisLoopStatus(RepeatMode mode) {
  switch (mode) {
    case RepeatMode::All:
      return "Playlist";
    case RepeatMode::One:
      return "Track";
    case RepeatMode::Off:
      break;
  }

  return "None";
}

/* ********************************************************************************************** */

std::optional<RepeatMode> ParseMprisLoopStatus(std::string_view status) {
  for (auto mode : {RepeatMode::Off, RepeatMode::All, RepeatMode::One}) {
    if (GetMprisLoopStatus(mode) == status) return mode;
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

std::string GetMprisTrackId(const PlayerStatus& status) {
  if (status.title.empty()) return std::string{kMprisNoTrack};

  return std::string{kTrackIdPrefix} + std::to_string(status.song_count);
}

/* ********************************************************************************************** */

std::string ToMprisFileUrl(std::string_view path) {
  return std::string{kFileScheme} + EncodeUri(path);
}

/* ********************************************************************************************** */

std::string GetMprisUrl(const PlayerStatus& status) {
  if (status.location.empty()) return {};

  if (StartsWith(status.location, kHttpScheme) || StartsWith(status.location, kHttpsScheme))
    return status.location;

  return ToMprisFileUrl(status.location);
}

/* ********************************************************************************************** */

std::string_view GetMprisImageExtension(std::string_view image) {
  return StartsWith(image, kPngSignature) ? ".png" : ".jpg";
}

/* ********************************************************************************************** */

bool CanMprisSeek(const PlayerStatus& status) { return status.duration > 0; }

/* ********************************************************************************************** */

int64_t ToMprisTime(uint32_t seconds) {
  return static_cast<int64_t>(seconds) * static_cast<int64_t>(kMicrosecondsPerSecond);
}

/* ********************************************************************************************** */

std::optional<RemoteRequest> CreateMprisSeek(int64_t offset) {
  const int seconds = ToSeconds(offset);
  if (seconds == 0) return std::nullopt;

  return RemoteRequest{RemoteCommand::Seek, RemoteNumber{.value = seconds, .relative = true}};
}

/* ********************************************************************************************** */

std::optional<RemoteRequest> CreateMprisSetPosition(const PlayerStatus& status,
                                                    std::string_view track, int64_t position) {
  // As stated by specification, request is ignored when it is not for the current song
  if (track != GetMprisTrackId(status)) return std::nullopt;
  if (position < 0 || position > ToMprisTime(status.duration)) return std::nullopt;

  return RemoteRequest{RemoteCommand::Seek,
                       RemoteNumber{.value = ToSeconds(position), .relative = false}};
}

/* ********************************************************************************************** */

RemoteRequest CreateMprisSetVolume(double volume) {
  const auto level = static_cast<int>(std::round(std::clamp(volume, 0., 1.) * kMaxVolume));

  return RemoteRequest{RemoteCommand::SetVolume, RemoteNumber{.value = level, .relative = false}};
}

/* ********************************************************************************************** */

std::optional<RemoteRequest> CreateMprisOpenUri(std::string_view uri) {
  if (StartsWith(uri, kHttpScheme) || StartsWith(uri, kHttpsScheme)) {
    return RemoteRequest{RemoteCommand::Play, std::string{uri}};
  }

  if (!StartsWith(uri, kFileScheme)) return std::nullopt;

  // Only files from this host are accepted (so path comes right after scheme)
  auto path = DecodeUri(uri.substr(kFileScheme.size()));
  if (!path || path->empty() || path->front() != kPathSeparator) return std::nullopt;

  return RemoteRequest{RemoteCommand::Play, *path};
}

/* ********************************************************************************************** */

MprisChanges GetMprisChanges(const PlayerStatus& before, const PlayerStatus& after) {
  MprisChanges changes;
  auto& properties = changes.properties;

  if (GetMprisPlaybackStatus(before.state) != GetMprisPlaybackStatus(after.state))
    properties.emplace_back(kMprisPlaybackStatus);

  if (before.repeat != after.repeat) properties.emplace_back(kMprisLoopStatus);
  if (before.shuffle != after.shuffle) properties.emplace_back(kMprisShuffle);
  if (before.volume != after.volume) properties.emplace_back(kMprisVolume);

  if (before.song_count != after.song_count || before.artist != after.artist ||
      before.title != after.title || before.duration != after.duration ||
      before.location != after.location || before.cover != after.cover)
    properties.emplace_back(kMprisMetadata);

  if (CanMprisSeek(before) != CanMprisSeek(after)) properties.emplace_back(kMprisCanSeek);

  // Position only moves forward while playing (and does not move while paused), so anything else
  // means that it was changed on the same song
  if (before.song_count == after.song_count && HasSong(before) && HasSong(after)) {
    const bool paused =
        before.state == Song::MediaState::Pause && after.state == Song::MediaState::Pause;

    changes.seeked = after.position < before.position ||
                     after.position > before.position + kMaxPositionStep ||
                     (paused && after.position != before.position);
  }

  return changes;
}

}  // namespace model
