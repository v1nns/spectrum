#include <gmock/gmock-actions.h>
#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>

#include <memory>

#include "audio/lyric/lyric_finder.h"
#include "mock/html_parser_mock.h"
#include "mock/url_fetcher_mock.h"
#include "model/application_error.h"
#include "model/song.h"
#include "util/logger.h"

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrEq;

/**
 * @brief Tests with LyricFinder class
 */
class LyricFinderTest : public ::testing::Test {
  // Using declaration
  using LyricFinder = std::unique_ptr<lyric::LyricFinder>;

 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override { Init(); }

  void TearDown() override { finder.reset(); }

  void Init() {
    // Create mocks
    UrlFetcherMock* uf_mock = new UrlFetcherMock();
    HtmlParserMock* hp_mock = new HtmlParserMock();

    // Create LyricFinder
    finder = lyric::LyricFinder::Create(uf_mock, hp_mock);
  }

  //! Getter for UrlFetcher (necessary as inner variable is an unique_ptr)
  auto GetFetcher() -> UrlFetcherMock* {
    return reinterpret_cast<UrlFetcherMock*>(finder->fetcher_.get());
  }

  //! Getter for HtmlParser (necessary as inner variable is an unique_ptr)
  auto GetParser() -> HtmlParserMock* {
    return reinterpret_cast<HtmlParserMock*>(finder->parser_.get());
  }

  //! Get number of search engines
  size_t GetNumberOfEngines() { return finder->engines_.size(); }

 protected:
  LyricFinder finder;  //!< Song lyrics finder
};

/* ********************************************************************************************** */

TEST_F(LyricFinderTest, SearchWithEmptyResult) {
  auto fetcher = GetFetcher();
  auto parser = GetParser();
  auto number_engines = GetNumberOfEngines();

  // Setup expectations
  EXPECT_CALL(*fetcher, Fetch(_, _)).Times(number_engines);
  EXPECT_CALL(*parser, Parse(_, _)).Times(number_engines);

  std::string artist{"Powfu"};
  std::string title{"abandoned house"};

  auto song_lyrics = finder->Search(artist, title);
  EXPECT_THAT(song_lyrics, Eq(model::SongLyric{}));
}

/* ********************************************************************************************** */

TEST_F(LyricFinderTest, SearchWithResultUsingGoogle) {
  auto fetcher = GetFetcher();
  auto parser = GetParser();

  const model::SongLyric raw{
      "A person who thinks all the time\n"
      "Has nothing to think about except thoughts\n"
      "So, he loses touch with reality\n"
      "And lives in a world of illusions\n\n"

      "By thoughts, I mean specifically, chatter in the skull\n"
      "Perpetual and compulsive repetition of words\n"
      "Of reckoning and calculating\n"
      "I'm not saying that thinking is bad\n"
      "Like everything else, It's useful in moderation\n"
      "A good servant, but a bad master\n\n"

      "And all so-called civilized peoples\n"
      "Have increasingly become crazy and self-destructive\n"
      "Because, through excessive thinking\n"
      "They have lost touch with reality\n"
      "That's to say\n"
      "We confuse signs\n"
      "With the real world\n",
  };

  // Setup expectations
  EXPECT_CALL(*fetcher, Fetch(_, _)).Times(1).WillOnce(Return(error::kSuccess));
  EXPECT_CALL(*parser, Parse(_, _)).Times(1).WillOnce(Return(raw));

  std::string artist{"INZO"};
  std::string title{"Overthinker"};

  const model::SongLyric expected{
      "A person who thinks all the time\n"
      "Has nothing to think about except thoughts\n"
      "So, he loses touch with reality\n"
      "And lives in a world of illusions\n",

      "By thoughts, I mean specifically, chatter in the skull\n"
      "Perpetual and compulsive repetition of words\n"
      "Of reckoning and calculating\n"
      "I'm not saying that thinking is bad\n"
      "Like everything else, It's useful in moderation\n"
      "A good servant, but a bad master\n",

      "And all so-called civilized peoples\n"
      "Have increasingly become crazy and self-destructive\n"
      "Because, through excessive thinking\n"
      "They have lost touch with reality\n"
      "That's to say\n"
      "We confuse signs\n"
      "With the real world\n",
  };

  auto song_lyrics = finder->Search(artist, title);
  EXPECT_THAT(song_lyrics, ElementsAreArray(expected));
}

/* ********************************************************************************************** */

TEST_F(LyricFinderTest, SearchWithResultUsingAZLyrics) {
  auto fetcher = GetFetcher();
  auto parser = GetParser();

  const model::SongLyric raw{
      "\r\n",
      "Pardon me, excusez-moi (I'm sorry)",
      "Yeah, I coulda made a better choice",
      "I mean, what the fuck?",
      "I'm sorry",
      "I'm fuckin' sorry",
      "Yeah",
      "\n",

      "I'm sorry, I'm sorry I don't see you more",
      "I'm sorry that the four minutes where you see your son could feel like a chore",
      "Sis', I'm sorry I'm your kin",
      "Sorry we ain't close as we should've been",
      "Sorry to my old friends",
      "The stories we coulda wrote if our egos didn't take the pen",
      "Sorry to the freaks I led on (nah, for real, I'm sorry)",
      "Who thought their life was gonna change 'cause I gave 'em head on",
      "But instead, I sped off, yeah, I know I'm dead wrong",
      "Sorry to the guys I had to hide",
      "Sorry to the girls I had to lie to",
      "Who ain't need to know if I was by the lake switchin' tides, too",
      "Anyway, I don't wanna talk",
      "Sorry if you gotta dig for info I don't wanna give",
      "So you stalk, make up fibs",
      "Just to talk 'bout my private life 'cause you weird (uh)",
      "Met that girl this year (but), that's none ya biz",
      "Give enough with my art, know your place",
      "My personal space, y'all don't need to to be a part",
      "I'm sorry I don't wanna link (I don't wanna link)",
      "And small talk over dinner, I don't even drink",
      "Can't guilt trip me, I'm ice cold, roller rink",
      "Nigga-nigga-nigga, read the room",
      "Don't assume niggas is cool",
      "Stay in your pocket, this is pool",
      "Blah, blah, blah, blah 'bout trauma",
      "You ain't special, everybody got problems, uh",
  };

  // Setup expectations
  EXPECT_CALL(*fetcher, Fetch(_, _))
      .Times(2)
      .WillOnce(Return(error::kUnknownError))
      .WillOnce(Return(error::kSuccess));

  EXPECT_CALL(*parser, Parse(_, _)).Times(1).WillOnce(Return(raw));

  std::string artist{"Tyler, the Creator"};
  std::string title{"SORRY NOT SORRY"};

  const model::SongLyric expected{
      "Pardon me, excusez-moi (I'm sorry)\n"
      "Yeah, I coulda made a better choice\n"
      "I mean, what the fuck?\n"
      "I'm sorry\n"
      "I'm fuckin' sorry\n"
      "Yeah\n",

      "I'm sorry, I'm sorry I don't see you more\n"
      "I'm sorry that the four minutes where you see your son could feel like a chore\n"
      "Sis', I'm sorry I'm your kin\n"
      "Sorry we ain't close as we should've been\n"
      "Sorry to my old friends\n"
      "The stories we coulda wrote if our egos didn't take the pen\n"
      "Sorry to the freaks I led on (nah, for real, I'm sorry)\n"
      "Who thought their life was gonna change 'cause I gave 'em head on\n"
      "But instead, I sped off, yeah, I know I'm dead wrong\n"
      "Sorry to the guys I had to hide\n"
      "Sorry to the girls I had to lie to\n"
      "Who ain't need to know if I was by the lake switchin' tides, too\n"
      "Anyway, I don't wanna talk\n"
      "Sorry if you gotta dig for info I don't wanna give\n"
      "So you stalk, make up fibs\n"
      "Just to talk 'bout my private life 'cause you weird (uh)\n"
      "Met that girl this year (but), that's none ya biz\n"
      "Give enough with my art, know your place\n"
      "My personal space, y'all don't need to to be a part\n"
      "I'm sorry I don't wanna link (I don't wanna link)\n"
      "And small talk over dinner, I don't even drink\n"
      "Can't guilt trip me, I'm ice cold, roller rink\n"
      "Nigga-nigga-nigga, read the room\n"
      "Don't assume niggas is cool\n"
      "Stay in your pocket, this is pool\n"
      "Blah, blah, blah, blah 'bout trauma\n"
      "You ain't special, everybody got problems, uh\n",
  };

  auto song_lyrics = finder->Search(artist, title);
  EXPECT_THAT(song_lyrics, ElementsAreArray(expected));
}

/* ********************************************************************************************** */

TEST_F(LyricFinderTest, ErrorOnFetch) {
  auto fetcher = GetFetcher();
  auto parser = GetParser();

  // Setup expectations
  EXPECT_CALL(*fetcher, Fetch(_, _)).Times(2).WillRepeatedly(Return(error::kUnknownError));
  EXPECT_CALL(*parser, Parse(_, _)).Times(0);

  std::string artist{"Funkin' Sound Team"};
  std::string title{"M.I.L.F"};

  const model::SongLyric expected{};

  auto song_lyrics = finder->Search(artist, title);
  EXPECT_THAT(song_lyrics, ElementsAreArray(expected));
}

/* ********************************************************************************************** */

TEST_F(LyricFinderTest, ErrorOnParse) {
  auto fetcher = GetFetcher();
  auto parser = GetParser();

  // Setup expectations
  EXPECT_CALL(*fetcher, Fetch(_, _)).Times(2).WillRepeatedly(Return(error::kSuccess));
  EXPECT_CALL(*parser, Parse(_, _)).Times(2).WillRepeatedly(Return(model::SongLyric{}));

  std::string artist{"Kaiser Chiefs"};
  std::string title{"Ruby"};

  const model::SongLyric expected{};

  auto song_lyrics = finder->Search(artist, title);
  EXPECT_THAT(song_lyrics, ElementsAreArray(expected));
}

/* ********************************************************************************************** */

TEST_F(LyricFinderTest, ErrorOnFormattingLyrics) {
  auto fetcher = GetFetcher();
  auto parser = GetParser();

  const std::string raw{
      "I can feel it now that you've gone\n"
      "I have made you all that I want\n"
      "I know you're keeping to your own sound\n"
      "You're running out of sight when the light goes down\n"
      "Said you'll be waiting 'till the night's done but there's no one\n"
      "And the world went on but I always knew you'd come\n"
      "Just one feeling, just one feeling\n"
      "Just one feeling, just one feeling then I know\n"
      "Just one feeling, just one feeling\n"
      "Just one feeling, just one feeling then I know\n"
      "Just one feeling, just one feeling\n"
      "Just one feeling, just one feeling\n"};

  // Setup expectations
  EXPECT_CALL(*fetcher, Fetch(_, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgReferee<1>(raw), Return(error::kSuccess)));

  EXPECT_CALL(*parser, Parse(StrEq(raw), _))
      .Times(2)
      .WillRepeatedly(Return(model::SongLyric{"\r\n", "\n"}));

  std::string artist{"Bombay Bicycle Club"};
  std::string title{"Feel"};

  const model::SongLyric expected{};

  auto song_lyrics = finder->Search(artist, title);
  EXPECT_THAT(song_lyrics, ElementsAreArray(expected));
}

}  // namespace
