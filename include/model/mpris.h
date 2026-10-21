/**
 * \file
 * \brief  Conversion between player status/commands and the values used by MPRIS (D-Bus)
 */

#ifndef INCLUDE_MODEL_MPRIS_H_
#define INCLUDE_MODEL_MPRIS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "model/player_status.h"
#include "model/remote_command.h"
#include "model/repeat_mode.h"
#include "model/song.h"

namespace model {

//! Track identifier used when there is no song
inline constexpr std::string_view kMprisNoTrack{"/org/mpris/MediaPlayer2/TrackList/NoTrack"};

//! Names of properties from player interface that are notified when changed
inline constexpr std::string_view kMprisPlaybackStatus{"PlaybackStatus"};
inline constexpr std::string_view kMprisLoopStatus{"LoopStatus"};
inline constexpr std::string_view kMprisShuffle{"Shuffle"};
inline constexpr std::string_view kMprisVolume{"Volume"};
inline constexpr std::string_view kMprisMetadata{"Metadata"};
inline constexpr std::string_view kMprisCanSeek{"CanSeek"};

/**
 * @brief Get playback status from song state
 * @param state Song state
 * @return "Playing", "Paused" or "Stopped" (any other state means there is nothing to play)
 */
std::string_view GetMprisPlaybackStatus(Song::MediaState state);

/**
 * @brief Get loop status from repeat mode
 * @param mode Repeat mode
 * @return "None", "Playlist" or "Track"
 */
std::string_view GetMprisLoopStatus(RepeatMode mode);

/**
 * @brief Get repeat mode from loop status
 * @param status Loop status ("None", "Playlist" or "Track")
 * @return Repeat mode (or nothing, if it is not a valid loop status)
 */
std::optional<RepeatMode> ParseMprisLoopStatus(std::string_view status);

/**
 * @brief Get identifier for current song (a D-Bus object path)
 * @param status Player status
 * @return Identifier that changes for each song loaded (or kMprisNoTrack, without a song)
 */
std::string GetMprisTrackId(const PlayerStatus& status);

/**
 * @brief Get URL for a local file
 * @param path Absolute path
 * @return URL ("file://" followed by path, with special characters encoded)
 */
std::string ToMprisFileUrl(std::string_view path);

/**
 * @brief Get URL for current song
 * @param status Player status
 * @return URL for local file, the original URL for streams (or empty, without a song)
 */
std::string GetMprisUrl(const PlayerStatus& status);

/**
 * @brief Get file extension for an image, from its content
 * @param image Content of image file
 * @return ".png" for a PNG image, otherwise ".jpg" (the format used by most covers)
 */
std::string_view GetMprisImageExtension(std::string_view image);

/**
 * @brief Check if current song accepts a new position (live streams do not have a duration)
 * @param status Player status
 * @return true if it accepts, otherwise false
 */
bool CanMprisSeek(const PlayerStatus& status);

/**
 * @brief Convert time to the unit used by MPRIS
 * @param seconds Time in seconds
 * @return Time in microseconds
 */
int64_t ToMprisTime(uint32_t seconds);

/**
 * @brief Create request to change position by offset
 * @param offset Offset in microseconds (negative to seek backward)
 * @return Remote request (or nothing, if offset is smaller than a second)
 */
std::optional<RemoteRequest> CreateMprisSeek(int64_t offset);

/**
 * @brief Create request to change position of current song
 * @param status Player status
 * @param track Identifier of the song that must be playing
 * @param position Position in microseconds
 * @return Remote request (or nothing, if it is another song or position is outside of it)
 */
std::optional<RemoteRequest> CreateMprisSetPosition(const PlayerStatus& status,
                                                    std::string_view track, int64_t position);

/**
 * @brief Create request to change volume
 * @param volume Volume from 0.0 to 1.0 (values outside of it are limited to it)
 * @return Remote request
 */
RemoteRequest CreateMprisSetVolume(double volume);

/**
 * @brief Create request to play the given URI
 * @param uri Local file ("file://" followed by an absolute path) or HTTP(S) URL
 * @return Remote request (or nothing, if URI is not supported)
 */
std::optional<RemoteRequest> CreateMprisOpenUri(std::string_view uri);

/**
 * @brief Changes from one player status to another that must be notified to MPRIS clients
 */
struct MprisChanges {
  std::vector<std::string> properties;  //!< Names of properties changed (position is not one)
  bool seeked = false;                  //!< Position has jumped (instead of just moving forward)
};

/**
 * @brief Compare two player status to find what must be notified
 * @param before Player status previously notified
 * @param after New player status
 * @return Changes to notify
 */
MprisChanges GetMprisChanges(const PlayerStatus& before, const PlayerStatus& after);

}  // namespace model
#endif  // INCLUDE_MODEL_MPRIS_H_
