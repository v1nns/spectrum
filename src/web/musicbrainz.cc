#include "web/musicbrainz.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <thread>
#include <utility>

#include "nlohmann/json.hpp"
#include "util/logger.h"
#include "util/url.h"

#ifndef SPECTRUM_DEBUG
#include "web/driver/curl_wrapper.h"
#else
#include "debug/dummy_fetcher.h"
#endif

namespace web {

namespace {

constexpr int kMillisecondsPerSecond = 1000;  //!< To convert track length to seconds

//! Get value from JSON entry, or fallback if it does not exist, is null or has an unexpected type
template <typename T>
T GetOr(const nlohmann::json& entry, const char* key, const T& fallback) {
  if (!entry.is_object()) return fallback;

  auto it = entry.find(key);
  if (it == entry.end() || it->is_null()) return fallback;

  try {
    return it->get<T>();
  } catch (const nlohmann::json::exception&) {
    return fallback;
  }
}

//! Get array from JSON entry (or nullptr, if it does not exist or it is not an array)
const nlohmann::json* GetArray(const nlohmann::json& entry, const char* key) {
  if (!entry.is_object()) return nullptr;

  auto it = entry.find(key);
  return it != entry.end() && it->is_array() ? &*it : nullptr;
}

//! Escape characters with special meaning in search query (it uses Lucene syntax)
std::string EscapeQuery(const std::string& text) {
  static constexpr std::string_view kSpecial = R"(+-&|!(){}[]^"~*?:\/)";

  std::string escaped;
  for (char c : text) {
    if (kSpecial.find(c) != std::string_view::npos) escaped += '\\';
    escaped += c;
  }

  return escaped;
}

}  // namespace

/* ********************************************************************************************** */

std::shared_ptr<MusicBrainz> MusicBrainz::Create(UrlFetcher* fetcher) {
#ifndef SPECTRUM_DEBUG
  auto ft = fetcher != nullptr ? std::unique_ptr<UrlFetcher>(fetcher)
                               : std::make_unique<driver::CURLWrapper>(std::string{kUserAgent});
#else
  auto ft = fetcher != nullptr ? std::unique_ptr<UrlFetcher>(fetcher)
                               : std::make_unique<driver::DummyFetcher>();
#endif

  // Simply extend the MusicBrainz class, as we do not want to expose the default constructor,
  // neither do we want to use std::make_shared explicitly calling operator new()
  struct MakeSharedEnabler : public MusicBrainz {
    explicit MakeSharedEnabler(std::unique_ptr<UrlFetcher>&& fetcher)
        : MusicBrainz(std::move(fetcher)) {}
  };

  return std::make_shared<MakeSharedEnabler>(std::move(ft));
}

/* ********************************************************************************************** */

MusicBrainz::MusicBrainz(std::unique_ptr<UrlFetcher>&& fetcher) : fetcher_{std::move(fetcher)} {}

/* ********************************************************************************************** */

error::Code MusicBrainz::SearchArtists(const std::string& query,
                                       std::vector<model::Artist>& artists,
                                       const std::atomic<bool>* cancel) {
  INFO("Search artists on MusicBrainz, query=", std::quoted(query));

  nlohmann::json content;
  const std::string path = "artist?query=" + util::EncodeUrlQuery(EscapeQuery(query)) +
                           "&limit=" + std::to_string(kMaxArtists);

  if (error::Code result = Request(path, content, cancel); result != error::kSuccess) {
    return result;
  }

  return ParseArtists(content, artists);
}

/* ********************************************************************************************** */

error::Code MusicBrainz::FetchReleases(const std::string& artist_id,
                                       std::vector<model::ReleaseGroup>& releases,
                                       const std::atomic<bool>* cancel) {
  INFO("Fetch releases from artist=", artist_id);

  std::vector<model::ReleaseGroup> fetched;
  int total = 0;

  // Releases are listed in pages, so request them until all of them are fetched
  for (int page = 0; page < kMaxPages; ++page) {
    nlohmann::json content;
    const std::string path = "release-group?artist=" + util::EncodeUrlQuery(artist_id) +
                             "&type=album%7Cep%7Csingle&limit=" + std::to_string(kPageSize) +
                             "&offset=" + std::to_string(fetched.size());

    if (error::Code result = Request(path, content, cancel); result != error::kSuccess) {
      return result;
    }

    const size_t before = fetched.size();
    if (error::Code result = ParseReleases(content, fetched, total); result != error::kSuccess) {
      return result;
    }

    // Nothing else to fetch (or nothing new, which would request the same page forever)
    if (fetched.size() == before || static_cast<int>(fetched.size()) >= total) break;
  }

  INFO("Fetched releases=", fetched.size(), " total=", total);

  releases = std::move(fetched);
  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code MusicBrainz::FetchTracks(const std::string& release_id,
                                     std::vector<model::Song>& tracks,
                                     const std::atomic<bool>* cancel) {
  INFO("Fetch tracks from release=", release_id);

  const std::string base = "release?release-group=" + util::EncodeUrlQuery(release_id) +
                           "&inc=recordings&limit=" + std::to_string(kMaxEditions);

  // Official edition is preferred, but some releases only have other ones (e.g. bootlegs)
  for (const std::string& filter : {std::string{"&status=official"}, std::string{}}) {
    nlohmann::json content;

    if (error::Code result = Request(base + filter, content, cancel); result != error::kSuccess) {
      return result;
    }

    std::vector<model::Song> parsed;
    if (error::Code result = ParseTracks(content, parsed); result != error::kSuccess) {
      return result;
    }

    if (!parsed.empty()) {
      tracks = std::move(parsed);
      return error::kSuccess;
    }
  }

  tracks.clear();
  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code MusicBrainz::Request(const std::string& path, nlohmann::json& output,
                                 const std::atomic<bool>* cancel) {
  std::scoped_lock lock{mutex_};

  const std::string url = std::string{kBaseUrl} + path + "&fmt=json";
  fetcher_->SetCancelCheck([cancel] { return cancel != nullptr && cancel->load(); });

  error::Code result = error::kUnknownError;

  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    // Server refuses requests sent too often
    auto wait = last_request_ + request_interval_ - std::chrono::steady_clock::now();
    if (attempt > 0) wait = std::max<std::chrono::steady_clock::duration>(wait, retry_interval_);

    if (!Wait(wait, cancel)) return error::kUnknownError;

    std::string content;
    result = fetcher_->Fetch(url, content);
    last_request_ = std::chrono::steady_clock::now();

    if (result == error::kSuccess) {
      output = nlohmann::json::parse(content, nullptr, /*allow_exceptions=*/false);

      if (output.is_discarded()) {
        ERROR("Could not parse content from MusicBrainz URL=", url);
        return error::kUnknownError;
      }

      return error::kSuccess;
    }

    // Only worth trying again when server is busy
    if (result != error::kUrlUnavailable) break;
    WARN("MusicBrainz is busy, attempt=", attempt + 1, " URL=", url);
  }

  return result;
}

/* ********************************************************************************************** */

bool MusicBrainz::Wait(std::chrono::steady_clock::duration duration,
                       const std::atomic<bool>* cancel) {
  const auto until = std::chrono::steady_clock::now() + duration;

  while (std::chrono::steady_clock::now() < until) {
    if (cancel != nullptr && *cancel) return false;

    std::this_thread::sleep_for(std::min<std::chrono::steady_clock::duration>(
        kCancelCheckInterval, until - std::chrono::steady_clock::now()));
  }

  return cancel == nullptr || !*cancel;
}

/* ********************************************************************************************** */

error::Code MusicBrainz::ParseArtists(const nlohmann::json& content,
                                      std::vector<model::Artist>& artists) {
  const nlohmann::json* entries = GetArray(content, "artists");

  if (entries == nullptr) {
    ERROR("Could not parse artists from MusicBrainz");
    return error::kUnknownError;
  }

  std::vector<model::Artist> parsed;

  for (const auto& entry : *entries) {
    model::Artist artist{
        .id = GetOr<std::string>(entry, "id", ""),
        .name = GetOr<std::string>(entry, "name", ""),
        .type = GetOr<std::string>(entry, "type", ""),
        .country = GetOr<std::string>(entry, "country", ""),
        .disambiguation = GetOr<std::string>(entry, "disambiguation", ""),
    };

    if (artist.id.empty() || artist.name.empty()) continue;
    parsed.push_back(std::move(artist));
  }

  INFO("Parsed artists from MusicBrainz, found=", parsed.size());

  artists = std::move(parsed);
  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code MusicBrainz::ParseReleases(const nlohmann::json& content,
                                       std::vector<model::ReleaseGroup>& releases, int& total) {
  const nlohmann::json* entries = GetArray(content, "release-groups");

  if (entries == nullptr) {
    ERROR("Could not parse releases from MusicBrainz");
    return error::kUnknownError;
  }

  total = GetOr<int>(content, "release-group-count", 0);

  for (const auto& entry : *entries) {
    model::ReleaseGroup release{
        .id = GetOr<std::string>(entry, "id", ""),
        .title = GetOr<std::string>(entry, "title", ""),
        .date = GetOr<std::string>(entry, "first-release-date", ""),
        .primary_type = GetOr<std::string>(entry, "primary-type", ""),
    };

    if (release.id.empty() || release.title.empty()) continue;

    if (const nlohmann::json* types = GetArray(entry, "secondary-types"); types != nullptr) {
      for (const auto& type : *types) {
        if (type.is_string()) release.secondary_types.push_back(type.get<std::string>());
      }
    }

    releases.push_back(std::move(release));
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code MusicBrainz::ParseTracks(const nlohmann::json& content,
                                     std::vector<model::Song>& tracks) {
  const nlohmann::json* editions = GetArray(content, "releases");

  if (editions == nullptr) {
    ERROR("Could not parse tracks from MusicBrainz");
    return error::kUnknownError;
  }

  // Count tracks from each edition, as most editions share the original track list (while some
  // of them are promotional ones with only a few tracks, or deluxe ones with bonus tracks)
  auto count_tracks = [](const nlohmann::json& edition) {
    size_t count = 0;
    for (const auto& medium : *GetArray(edition, "media")) {
      if (const nlohmann::json* entries = GetArray(medium, "tracks"); entries != nullptr) {
        count += entries->size();
      }
    }
    return count;
  };

  std::map<size_t, int> counts;
  for (const auto& edition : *editions) {
    if (GetArray(edition, "media") != nullptr) counts[count_tracks(edition)]++;
  }

  // Most common track list (the longest one, when there is a tie)
  size_t common = 0;
  int occurrences = 0;

  for (const auto& [count, times] : counts) {
    if (times >= occurrences) {
      common = count;
      occurrences = times;
    }
  }

  // Earliest edition with that track list is the original one (editions without date are the last
  // option)
  const nlohmann::json* earliest = nullptr;
  std::string earliest_date;

  for (const auto& edition : *editions) {
    if (GetArray(edition, "media") == nullptr || count_tracks(edition) != common) continue;

    const std::string date = GetOr<std::string>(edition, "date", "");
    const bool earlier = !date.empty() && (earliest_date.empty() || date < earliest_date);

    if (earliest == nullptr || earlier) {
      earliest = &edition;
      earliest_date = date;
    }
  }

  std::vector<model::Song> parsed;

  if (earliest != nullptr) {
    for (const auto& medium : *GetArray(*earliest, "media")) {
      const nlohmann::json* entries = GetArray(medium, "tracks");
      if (entries == nullptr) continue;

      for (const auto& entry : *entries) {
        const std::string title = GetOr<std::string>(entry, "title", "");
        if (title.empty()) continue;

        // Length is in milliseconds (and it may be unknown)
        const auto length = GetOr<double>(entry, "length", 0);

        parsed.push_back(model::Song{
            .title = title,
            .duration = static_cast<uint32_t>(std::lround(length / kMillisecondsPerSecond)),
        });
      }
    }

    INFO("Parsed tracks=", parsed.size(), " from edition=", GetOr<std::string>(*earliest, "id", ""),
         " date=", earliest_date);
  }

  tracks = std::move(parsed);
  return error::kSuccess;
}

}  // namespace web
