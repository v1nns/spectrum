#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <memory>

#include "audio/lyric/lyric_finder.h"
#include "general/sync_testing.h"
#include "mock/html_parser_mock.h"
#include "mock/url_fetcher_mock.h"
#include "model/application_error.h"
#include "util/logger.h"

namespace {

using ::testing::_;

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

TEST_F(LyricFinderTest, EmptyResultOnSearch) {
  auto fetcher = GetFetcher();
  auto parser = GetParser();
  auto number_engines = GetNumberOfEngines();

  // Setup expectations
  EXPECT_CALL(*fetcher, Fetch(_, _)).Times(number_engines);
  EXPECT_CALL(*parser, Parse(_, _)).Times(number_engines);

  std::string artist{"Powfu"};
  std::string title{"abandoned house"};

  finder->Search(artist, title);
}

/* ********************************************************************************************** */

// TODO:
// Invoke on parse (to fill song lyrics, on first and second engine)
// Return error on fetch

}  // namespace
