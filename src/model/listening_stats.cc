#include "model/listening_stats.h"

#include <algorithm>
#include <tuple>

namespace model {

bool ListeningStats::Song::operator==(const Song& other) const {
  return std::tie(artist, title, listened_ms, plays) ==
         std::tie(other.artist, other.title, other.listened_ms, other.plays);
}

/* ********************************************************************************************** */

void ListeningStats::AddPlayed(const std::string& key, const std::string& artist,
                               const std::string& title, int64_t now) {
  if (since == 0) since = now;
  songs_played++;

  // Least listened song gives room to the new one (it would be the first one removed anyway)
  if (songs.find(key) == songs.end() && songs.size() >= kMaxSongs) {
    songs.erase(std::min_element(songs.begin(), songs.end(), [](const auto& lhs, const auto& rhs) {
      return lhs.second.listened_ms < rhs.second.listened_ms;
    }));
  }

  // Song information may change (e.g. tags edited), so keep the latest one
  Song& song = songs[key];
  song.artist = artist;
  song.title = title;
  song.plays++;
}

/* ********************************************************************************************** */

void ListeningStats::AddListened(const std::string& key, bool from_url, uint64_t ms) {
  listened_ms += ms;
  (from_url ? stream_ms : local_ms) += ms;

  if (auto song = songs.find(key); song != songs.end()) song->second.listened_ms += ms;
}

/* ********************************************************************************************** */

std::vector<ListeningStats::Song> ListeningStats::GetTopSongs(std::size_t count) const {
  std::vector<Song> top;
  top.reserve(songs.size());

  for (const auto& [key, song] : songs) {
    if (song.listened_ms > 0) top.push_back(song);
  }

  // Stable, so songs listened to for the same time keep the same order every time
  std::stable_sort(top.begin(), top.end(), [](const Song& lhs, const Song& rhs) {
    return lhs.listened_ms > rhs.listened_ms;
  });

  if (top.size() > count) top.resize(count);
  return top;
}

/* ********************************************************************************************** */

std::vector<ListeningStats::Artist> ListeningStats::GetTopArtists(std::size_t count) const {
  std::map<std::string, uint64_t> artists;

  for (const auto& [key, song] : songs) {
    if (!song.artist.empty() && song.listened_ms > 0) artists[song.artist] += song.listened_ms;
  }

  std::vector<Artist> top;
  top.reserve(artists.size());

  for (const auto& [name, listened_ms] : artists) {
    top.push_back(Artist{.name = name, .listened_ms = listened_ms});
  }

  std::stable_sort(top.begin(), top.end(), [](const Artist& lhs, const Artist& rhs) {
    return lhs.listened_ms > rhs.listened_ms;
  });

  if (top.size() > count) top.resize(count);
  return top;
}

/* ********************************************************************************************** */

bool ListeningStats::operator==(const ListeningStats& other) const {
  return std::tie(since, listened_ms, local_ms, stream_ms, songs_played, songs_finished,
                  songs_skipped, stream_bytes,
                  songs) == std::tie(other.since, other.listened_ms, other.local_ms,
                                     other.stream_ms, other.songs_played, other.songs_finished,
                                     other.songs_skipped, other.stream_bytes, other.songs);
}

}  // namespace model
