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

  static constexpr int kFieldColumns = 16;  //!< Columns for the name of each stat
  static constexpr int kTimeColumns = 11;   //!< Columns for time listened on top lists
  static constexpr int kColumnGap = 3;      //!< Columns between both top lists

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
   * @brief Save stats from all time to file
   */
  void Save() const;

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
   * @brief Renders lists with artists and songs listened to the most
   * @param stats Stats to show
   * @return UI element
   */
  ftxui::Element DrawTopLists(const model::ListeningStats& stats) const;

  /* ******************************************************************************************** */
  //! Variables

  //! Song playing right now
  struct Current {
    std::string key;        //!< Song identifier (filepath or URL)
    bool from_url = false;  //!< Song is played from URL
    bool finished = false;  //!< Song was played until its end
  };

  std::shared_ptr<util::FileHandler> file_handler_;  //!< Load/save stats from all time

  model::ListeningStats all_time_;  //!< Stats from all time (saved between runs)
  model::ListeningStats session_;   //!< Stats from this session only
  int64_t session_start_ = 0;       //!< When this session started (seconds since epoch)

  bool show_all_time_ = true;  //!< Show stats from all time (otherwise, only from this session)

  std::optional<Current> current_;         //!< Song playing (if any)
  std::optional<uint32_t> last_position_;  //!< Last position from song playing (in milliseconds)

  ftxui::Box all_time_box_;  //!< Box to click to show stats from all time
  ftxui::Box session_box_;   //!< Box to click to show stats from this session

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::MainContentTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_MAIN_CONTENT_LISTENING_STATISTICS_H_
