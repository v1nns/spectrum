/**
 * \file
 * \brief  Class for tab view containing listening stats
 */

#ifndef INCLUDE_VIEW_BLOCK_MAIN_CONTENT_LISTENING_STATISTICS_H_
#define INCLUDE_VIEW_BLOCK_MAIN_CONTENT_LISTENING_STATISTICS_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ftxui/screen/box.hpp"
#include "model/listening_stats.h"
#include "util/file_handler.h"
#include "view/element/tab.h"

#ifdef ENABLE_TESTS
namespace {
class MainContentTest;
}
#endif

namespace interface {

/**
 * @brief Component to render stats about what was listened to, from all time (kept between runs)
 * or only from this session
 */
class ListeningStatistics : public TabItem {
  static constexpr std::string_view kTabName = "stats";  //!< Tab title
  static constexpr std::size_t kTopCount = 5;            //!< Entries from each top list

  //! Biggest change of song position counted as listened (anything else is a seek)
  static constexpr uint32_t kMaxPositionStep = 500;

  //! Time listened to a song before counting it as played (unless it finishes before that)
  static constexpr uint64_t kPlayedThreshold = 30000;

  //! Time listened before saving stats again (besides saving when each song stops)
  static constexpr uint64_t kSaveInterval = 300000;

  static constexpr int kFieldColumns = 16;  //!< Columns for the name of each stat
  static constexpr int kTimeColumns = 11;   //!< Columns for time listened on top lists
  static constexpr int kColumnGap = 3;      //!< Columns between both top lists (and other columns)
  static constexpr int kHourLabelStep = 6;  //!< Hours between each label below hours listened

 public:
  /**
   * @brief Construct a new ListeningStatistics object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   * @param on_focus Callback function to ask for focus
   * @param keybinding Keybinding to set item as active
   * @param file_handler Interface to load/save listening stats
   */
  explicit ListeningStatistics(const model::BlockIdentifier& id,
                               const std::shared_ptr<EventDispatcher>& dispatcher,
                               const FocusCallback& on_focus, const keybinding::Key& keybinding,
                               const std::shared_ptr<util::FileHandler>& file_handler);

  /**
   * @brief Destroy the ListeningStatistics object (saving stats from all time)
   */
  ~ListeningStatistics() override;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event) override;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEvent(ftxui::Event& event) override;

  /**
   * @brief Handles a custom event (always returns false, as other blocks also use them)
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
  //! Private methods
 private:
  /**
   * @brief Apply the same change on stats from all time and from this session
   * @param change Function changing stats
   */
  template <typename Change>
  void Update(Change&& change) {
    change(all_time_);
    change(session_);
  }

  /**
   * @brief Save stats from all time to file, adding only what was counted since the last time it
   * was saved to what is in it right now (as another instance may have saved it in the meantime)
   */
  void Save();

  /**
   * @brief Renders tabs to choose between stats from all time and from this session
   * @return UI element
   */
  ftxui::Element DrawScope();

  /**
   * @brief Renders a single stat (its name and value)
   * @param field Stat name
   * @param value Stat value
   * @return UI element
   */
  ftxui::Element DrawField(const std::string& field, const std::string& value) const;

  /**
   * @brief Scroll content (when it does not fit in the tab)
   * @param rows Rows to scroll (negative scrolls up)
   * @return true if content was scrolled, otherwise false
   */
  bool Scroll(int rows);

  /**
   * @brief Renders totals from stats (e.g. time listened, songs played)
   * @param stats Stats to show
   * @return UI element
   */
  ftxui::Element DrawTotals(const model::ListeningStats& stats) const;

  /**
   * @brief Renders time listened on each hour from each weekday
   * @param stats Stats to show
   * @return UI element
   */
  ftxui::Element DrawHours(const model::ListeningStats& stats) const;

  /**
   * @brief Renders lists with artists and songs listened to the most
   * @param stats Stats to show
   * @return UI element
   */
  ftxui::Element DrawTopLists(const model::ListeningStats& stats) const;

  /* ******************************************************************************************** */
  //! Variables

  //! Song playing right now
  struct Current {
    std::string key;           //!< Song identifier (filepath or URL)
    std::string artist;        //!< Song artist
    std::string title;         //!< Song title (or filename/URL, when it has no title)
    bool from_url = false;     //!< Song is played from URL
    uint64_t listened_ms = 0;  //!< Time listened to it (in milliseconds)
    bool played = false;       //!< Song was already counted as played
    bool finished = false;     //!< Song was played until its end
  };

  std::shared_ptr<util::FileHandler> file_handler_;  //!< Load/save stats from all time

  model::ListeningStats all_time_;  //!< Stats from all time (saved between runs)
  model::ListeningStats session_;   //!< Stats from this session only
  model::ListeningStats saved_;     //!< Stats from all time, as they were saved the last time
  uint64_t unsaved_ms_ = 0;         //!< Time listened since stats were saved (in milliseconds)
  int64_t session_start_ = 0;       //!< When this session started (seconds since epoch)

  bool show_all_time_ = true;  //!< Show stats from all time (otherwise, only from this session)

  std::optional<Current> current_;         //!< Song playing (if any)
  std::optional<uint32_t> last_position_;  //!< Last position from song playing (in milliseconds)

  ftxui::Box all_time_box_;  //!< Box to click to show stats from all time
  ftxui::Box session_box_;   //!< Box to click to show stats from this session

  int scroll_ = 0;          //!< Rows scrolled from the top of content
  ftxui::Box view_box_;     //!< Box with visible part of content
  ftxui::Box content_box_;  //!< Box with whole content (even what is not visible)

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::MainContentTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_MAIN_CONTENT_LISTENING_STATISTICS_H_
