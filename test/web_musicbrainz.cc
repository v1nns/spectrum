#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "mock/url_fetcher_mock.h"
#include "model/application_error.h"
#include "model/artist_search.h"
#include "model/song.h"
#include "util/logger.h"
#include "web/musicbrainz.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::StrEq;

/**
 * @brief Tests with MusicBrainz class (without fetching anything from network)
 */
class MusicBrainzTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override {
    musicbrainz = web::MusicBrainz::Create(new UrlFetcherMock());

    // Do not wait between requests (unless test asks for it)
    SetIntervals(std::chrono::milliseconds{0}, std::chrono::milliseconds{0});
  }

  //! Getter for UrlFetcher (necessary as inner variable is an unique_ptr)
  UrlFetcherMock* GetFetcher() {
    return reinterpret_cast<UrlFetcherMock*>(musicbrainz->fetcher_.get());
  }

  //! Set minimum interval between requests, and interval to try again while server is busy
  void SetIntervals(std::chrono::milliseconds request, std::chrono::milliseconds retry) {
    musicbrainz->request_interval_ = request;
    musicbrainz->retry_interval_ = retry;
  }

  //! Action to reply with the given content
  static auto Reply(const std::string& content) {
    return DoAll(Invoke([content](const std::string&, std::string& output) { output += content; }),
                 Return(error::kSuccess));
  }

  //! Matcher for track title and duration
  static auto Track(const std::string& title, uint32_t duration) {
    return AllOf(Field(&model::Song::title, StrEq(title)), Field(&model::Song::duration, duration));
  }

  std::shared_ptr<web::MusicBrainz> musicbrainz;  //!< Class under test
};

/* ********************************************************************************************** */

TEST_F(MusicBrainzTest, SearchArtists) {
  // Based on a real reply, without most fields
  EXPECT_CALL(
      *GetFetcher(),
      Fetch(StrEq("https://musicbrainz.org/ws/2/artist?query=sigur%20r%C3%B3s%20%5C%28is%5C%29"
                  "&limit=8&fmt=json"),
            _))
      .WillOnce(Reply(R"({"count": 3, "artists": [
        {"id": "f6f2326f", "type": "Group", "score": 100, "name": "Sigur Rós", "country": "IS",
         "disambiguation": "Icelandic post-rock band"},
        {"id": "7d45ae1b", "score": 80, "name": "Sigur"},
        {"type": "Person", "name": "Without identifier"}
      ]})"));

  // Characters with special meaning in search query are escaped, and everything is encoded
  std::vector<model::Artist> artists;
  ASSERT_EQ(musicbrainz->SearchArtists("sigur rós (is)", artists), error::kSuccess);

  EXPECT_THAT(artists, ElementsAre(
                           model::Artist{
                               .id = "f6f2326f",
                               .name = "Sigur Rós",
                               .type = "Group",
                               .country = "IS",
                               .disambiguation = "Icelandic post-rock band",
                           },
                           model::Artist{.id = "7d45ae1b", .name = "Sigur"}));
}

/* ********************************************************************************************** */

TEST_F(MusicBrainzTest, FetchReleasesFromAllPages) {
  const std::string url =
      "https://musicbrainz.org/ws/2/release-group?artist=f6f2326f&type=album%7Cep%7Csingle"
      "&limit=100&offset=";

  // Releases are listed in pages, until all of them are fetched
  EXPECT_CALL(*GetFetcher(), Fetch(StrEq(url + "0&fmt=json"), _))
      .WillOnce(Reply(R"({"release-group-count": 3, "release-groups": [
        {"id": "rg1", "title": "Takk…", "first-release-date": "2005-09-12",
         "primary-type": "Album", "secondary-types": []},
        {"id": "rg2", "title": "Inni", "first-release-date": "2011",
         "primary-type": "Album", "secondary-types": ["Live"]}
      ]})"));

  EXPECT_CALL(*GetFetcher(), Fetch(StrEq(url + "2&fmt=json"), _))
      .WillOnce(Reply(R"({"release-group-count": 3, "release-groups": [
        {"id": "rg3", "title": "Without date", "primary-type": null}
      ]})"));

  std::vector<model::ReleaseGroup> releases;
  ASSERT_EQ(musicbrainz->FetchReleases("f6f2326f", releases), error::kSuccess);

  EXPECT_THAT(releases, ElementsAre(
                            model::ReleaseGroup{
                                .id = "rg1",
                                .title = "Takk…",
                                .date = "2005-09-12",
                                .primary_type = "Album",
                            },
                            model::ReleaseGroup{
                                .id = "rg2",
                                .title = "Inni",
                                .date = "2011",
                                .primary_type = "Album",
                                .secondary_types = {"Live"},
                            },
                            model::ReleaseGroup{.id = "rg3", .title = "Without date"}));
}

/* ********************************************************************************************** */

TEST_F(MusicBrainzTest, FetchTracksFromOriginalEdition) {
  // Promotional edition is the earliest one, but most editions have the original track list (and
  // edition with a full date is the earliest one among them)
  EXPECT_CALL(*GetFetcher(), Fetch(StrEq("https://musicbrainz.org/ws/2/release?release-group=rg1"
                                         "&inc=recordings&limit=25&status=official&fmt=json"),
                                   _))
      .WillOnce(Reply(R"({"release-count": 4, "releases": [
        {"id": "deluxe", "date": "2013-06-14", "media": [
          {"tracks": [{"title": "Brennisteinn", "length": 465000}]},
          {"tracks": [{"title": "Hrafntinna", "length": 383000},
                      {"title": "Bonus", "length": 100000}]}]},
        {"id": "promo", "date": "2013", "media": [
          {"tracks": [{"title": "Brennisteinn", "length": 465000}]}]},
        {"id": "late", "date": "2013-06-14", "media": [
          {"tracks": [{"title": "Brennisteinn", "length": 465000}]},
          {"tracks": [{"title": "Hrafntinna", "length": 383000}]}]},
        {"id": "original", "date": "2013-06-12", "media": [
          {"tracks": [{"title": "Brennisteinn", "length": 465400},
                      {"title": "Hrafntinna", "length": null}]}]},
        {"id": "without date", "media": [
          {"tracks": [{"title": "Other", "length": 1000}, {"title": "Other", "length": 1000}]}]}
      ]})"));

  std::vector<model::Song> tracks;
  ASSERT_EQ(musicbrainz->FetchTracks("rg1", tracks), error::kSuccess);

  // Length is rounded to seconds (and it may be unknown)
  EXPECT_THAT(tracks, ElementsAre(Track("Brennisteinn", 465), Track("Hrafntinna", 0)));
}

/* ********************************************************************************************** */

TEST_F(MusicBrainzTest, FetchTracksWithoutOfficialEdition) {
  const std::string url =
      "https://musicbrainz.org/ws/2/release?release-group=rg2&inc=recordings&limit=25";

  // Only unofficial editions (e.g. a bootleg from a concert)
  EXPECT_CALL(*GetFetcher(), Fetch(StrEq(url + "&status=official&fmt=json"), _))
      .WillOnce(Reply(R"({"release-count": 0, "releases": []})"));

  EXPECT_CALL(*GetFetcher(), Fetch(StrEq(url + "&fmt=json"), _))
      .WillOnce(Reply(R"({"release-count": 1, "releases": [
        {"id": "bootleg", "media": [{"tracks": [{"title": "Vaka", "length": 400000}]}]}
      ]})"));

  std::vector<model::Song> tracks;
  ASSERT_EQ(musicbrainz->FetchTracks("rg2", tracks), error::kSuccess);
  EXPECT_THAT(tracks, ElementsAre(Track("Vaka", 400)));
}

/* ********************************************************************************************** */

TEST_F(MusicBrainzTest, RetryWhileServerIsBusy) {
  auto busy = DoAll(Invoke([](const std::string&, std::string& output) { output += "busy"; }),
                    Return(error::kUrlUnavailable));

  // Content from a failed attempt is not mixed with content from next one
  EXPECT_CALL(*GetFetcher(), Fetch(_, _))
      .WillOnce(busy)
      .WillOnce(busy)
      .WillOnce(Reply(R"({"artists": [{"id": "a1", "name": "Cartola"}]})"));

  std::vector<model::Artist> artists;
  ASSERT_EQ(musicbrainz->SearchArtists("cartola", artists), error::kSuccess);
  EXPECT_THAT(artists, ElementsAre(model::Artist{.id = "a1", .name = "Cartola"}));

  // Until it gives up
  EXPECT_CALL(*GetFetcher(), Fetch(_, _)).Times(3).WillRepeatedly(busy);
  EXPECT_EQ(musicbrainz->SearchArtists("cartola", artists), error::kUrlUnavailable);
}

/* ********************************************************************************************** */

TEST_F(MusicBrainzTest, ErrorOnFetchOrParse) {
  std::vector<model::Artist> artists;

  // Any other error is not worth trying again
  EXPECT_CALL(*GetFetcher(), Fetch(_, _)).WillOnce(Return(error::kUnknownError));
  EXPECT_EQ(musicbrainz->SearchArtists("cartola", artists), error::kUnknownError);

  EXPECT_CALL(*GetFetcher(), Fetch(_, _)).WillOnce(Reply("<html>not json</html>"));
  EXPECT_EQ(musicbrainz->SearchArtists("cartola", artists), error::kUnknownError);

  EXPECT_CALL(*GetFetcher(), Fetch(_, _)).WillOnce(Reply(R"({"unexpected": []})"));
  EXPECT_EQ(musicbrainz->SearchArtists("cartola", artists), error::kUnknownError);

  EXPECT_THAT(artists, IsEmpty());
}

/* ********************************************************************************************** */

TEST_F(MusicBrainzTest, WaitBetweenRequests) {
  static constexpr std::chrono::milliseconds kInterval{150};
  SetIntervals(kInterval, std::chrono::milliseconds{0});

  EXPECT_CALL(*GetFetcher(), Fetch(_, _))
      .Times(2)
      .WillRepeatedly(Invoke([](const std::string&, std::string& output) {
        output += R"({"artists": []})";
        return error::kSuccess;
      }));

  std::vector<model::Artist> artists;
  ASSERT_EQ(musicbrainz->SearchArtists("cartola", artists), error::kSuccess);

  const auto start = std::chrono::steady_clock::now();
  ASSERT_EQ(musicbrainz->SearchArtists("cartola", artists), error::kSuccess);

  EXPECT_GE(std::chrono::steady_clock::now() - start, kInterval);
}

/* ********************************************************************************************** */

TEST_F(MusicBrainzTest, CancelWhileWaiting) {
  SetIntervals(std::chrono::seconds{30}, std::chrono::milliseconds{0});

  EXPECT_CALL(*GetFetcher(), Fetch(_, _))
      .WillOnce(Reply(R"({"artists": [{"id": "a1", "name": "Cartola"}]})"));

  std::vector<model::Artist> artists;
  ASSERT_EQ(musicbrainz->SearchArtists("cartola", artists), error::kSuccess);

  // Next request would wait for a long time, but it is canceled right away
  std::atomic<bool> cancel = true;
  const auto start = std::chrono::steady_clock::now();

  EXPECT_EQ(musicbrainz->SearchArtists("cartola", artists, &cancel), error::kUnknownError);
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds{1});
}

}  // namespace
