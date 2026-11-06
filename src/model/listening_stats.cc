#include "model/listening_stats.h"

#include <algorithm>
#include <ctime>
#include <tuple>

namespace model {

namespace {

//! Convert moment (seconds since epoch) to local time
std::tm ToLocalTime(int64_t seconds) {
  const auto moment = static_cast<std::time_t>(seconds);
  std::tm local{};
  localtime_r(&moment, &local);
  return local;
}

//! Get day from moment in local time (days since epoch)
int64_t ToLocalDay(int64_t seconds) {
  constexpr int64_t kDaySeconds = 24 * 60 * 60;
  return (seconds + ToLocalTime(seconds).tm_gmtoff) / kDaySeconds;
}

}  // namespace

/* ********************************************************************************************** */

bool ListeningStats::Streak::operator==(const Streak& other) const {
  return std::tie(day, current, longest) == std::tie(other.day, other.current, other.longest);
}

/* ********************************************************************************************** */

bool ListeningStats::Song::operator==(const Song& other) const {
  return std::tie(artist, title, listened_ms, plays) ==
         std::tie(other.artist, other.title, other.listened_ms, other.plays);
}

/* ********************************************************************************************** */

void ListeningStats::AddListened(const std::string& key, const std::string& artist,
                                 const std::string& title, bool from_url, uint64_t ms,
                                 int64_t now) {
  if (since == 0) since = now;

  listened_ms += ms;
  (from_url ? stream_ms : local_ms) += ms;

  // Weekday from std::tm starts on Sunday, but here it starts on Monday
  const std::tm local = ToLocalTime(now);
  const auto weekday = static_cast<std::size_t>(local.tm_wday + kWeekdays - 1) % kWeekdays;
  hours[weekday][static_cast<std::size_t>(local.tm_hour)] += ms;

  // Least listened song gives room to the new one (it would be the first one removed anyway)
  if (songs.find(key) == songs.end() && songs.size() >= kMaxSongs) RemoveLeastListened();

  // Song information may change (e.g. tags edited), so keep the latest one
  Song& song = songs[key];
  song.artist = artist;
  song.title = title;
  song.listened_ms += ms;
}

/* ********************************************************************************************** */

void ListeningStats::AddPlayed(const std::string& key, int64_t now) {
  songs_played++;
  if (auto song = songs.find(key); song != songs.end()) song->second.plays++;

  // Day already counted (or earlier than it, when clock goes back) does not change streak
  const int64_t today = ToLocalDay(now);
  if (streak.current > 0 && today <= streak.day) return;

  streak.current = streak.current > 0 && today == streak.day + 1 ? streak.current + 1 : 1;
  streak.day = today;
  streak.longest = std::max(streak.longest, streak.current);
}

/* ********************************************************************************************** */

uint32_t ListeningStats::GetCurrentStreak(int64_t now) const {
  // Streak is only broken after a whole day without songs played
  return ToLocalDay(now) <= streak.day + 1 ? streak.current : 0;
}

/* ********************************************************************************************** */

ListeningStats ListeningStats::Difference(const ListeningStats& before) const {
  // Values only grow, but a song may have been removed and counted again from zero in the meantime
  auto minus = [](auto value, auto earlier) { return value > earlier ? value - earlier : 0; };

  ListeningStats diff{
      .since = before.since == 0 ? since : 0,
      .listened_ms = minus(listened_ms, before.listened_ms),
      .local_ms = minus(local_ms, before.local_ms),
      .stream_ms = minus(stream_ms, before.stream_ms),
      .songs_played = minus(songs_played, before.songs_played),
      .songs_finished = minus(songs_finished, before.songs_finished),
      .songs_skipped = minus(songs_skipped, before.songs_skipped),
      .stream_bytes = minus(stream_bytes, before.stream_bytes),
      // Streak is not a total, so it is carried as it is (only when it changed)
      .streak = streak != before.streak ? streak : Streak{},
  };

  for (std::size_t day = 0; day < kWeekdays; day++) {
    for (std::size_t hour = 0; hour < kDayHours; hour++) {
      diff.hours[day][hour] = minus(hours[day][hour], before.hours[day][hour]);
    }
  }

  for (const auto& [key, song] : songs) {
    auto earlier = before.songs.find(key);
    const bool existed = earlier != before.songs.end();

    Song changed{
        .artist = song.artist,
        .title = song.title,
        .listened_ms =
            existed ? minus(song.listened_ms, earlier->second.listened_ms) : song.listened_ms,
        .plays = existed ? minus(song.plays, earlier->second.plays) : song.plays,
    };

    if (changed.listened_ms > 0 || changed.plays > 0) diff.songs[key] = std::move(changed);
  }

  return diff;
}

/* ********************************************************************************************** */

void ListeningStats::Add(const ListeningStats& other) {
  if (other.since != 0 && (since == 0 || other.since < since)) since = other.since;

  listened_ms += other.listened_ms;
  local_ms += other.local_ms;
  stream_ms += other.stream_ms;
  songs_played += other.songs_played;
  songs_finished += other.songs_finished;
  songs_skipped += other.songs_skipped;
  stream_bytes += other.stream_bytes;

  for (std::size_t day = 0; day < kWeekdays; day++) {
    for (std::size_t hour = 0; hour < kDayHours; hour++) hours[day][hour] += other.hours[day][hour];
  }

  // Streak with the latest day with a song played is the current one (both may have played songs
  // on the same day, then the one with more days in a row is kept)
  if (other.streak.day > streak.day) {
    streak.day = other.streak.day;
    streak.current = other.streak.current;
  } else if (other.streak.day == streak.day) {
    streak.current = std::max(streak.current, other.streak.current);
  }

  streak.longest = std::max(streak.longest, other.streak.longest);

  for (const auto& [key, song] : other.songs) {
    Song& total = songs[key];
    total.artist = song.artist;
    total.title = song.title;
    total.listened_ms += song.listened_ms;
    total.plays += song.plays;
  }

  while (songs.size() > kMaxSongs) RemoveLeastListened();
}

/* ********************************************************************************************** */

void ListeningStats::RemoveLeastListened() {
  songs.erase(std::min_element(songs.begin(), songs.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.second.listened_ms < rhs.second.listened_ms;
  }));
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
                  songs_skipped, stream_bytes, songs, hours, streak) ==
         std::tie(other.since, other.listened_ms, other.local_ms, other.stream_ms,
                  other.songs_played, other.songs_finished, other.songs_skipped, other.stream_bytes,
                  other.songs, other.hours, other.streak);
}

}  // namespace model
