/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_
#define INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>  // for path
#include <memory>      // for shared_ptr
#include <mutex>
#include <optional>  // for optional
#include <string>    // for string, allocator
#include <string_view>
#include <thread>
#include <vector>  // for vector

#include "ftxui/component/captured_mouse.hpp"     // for ftxui
#include "ftxui/component/component_options.hpp"  // for MenuEntryOption
#include "ftxui/dom/elements.hpp"                 // for Element
#include "ftxui/screen/box.hpp"                   // for Box
#include "util/file_handler.h"
#include "view/base/block.h"  // for Block, BlockEvent...

#ifdef ENABLE_TESTS
#include <gtest/gtest_prod.h>

//! Forward declaration
namespace {
class ListDirectoryTest;
class ListDirectoryTest_RunTextAnimation_Test;
class ListDirectoryTest_ScrollMenuOnBigList_Test;
class ListDirectoryTest_TabMenuOnBigList_Test;
class ListDirectoryTest_PlayNextFileAfterFinished_Test;
class ListDirectoryTest_StartPlayingLastFileAndPlayNextAfterFinished_Test;
class ListDirectoryCtorTest;
class ListDirectoryCtorTest_CreateWithBadInitialPath_Test;
}  // namespace
#endif

namespace interface {

//! Custom style for menu entry
struct MenuEntryOption {
  ftxui::Decorator normal;
  ftxui::Decorator focused;
  ftxui::Decorator selected;
  ftxui::Decorator selected_focused;
};

//! Create a new custom style for Menu Entry
inline MenuEntryOption Colored(ftxui::Color c) {
  using ftxui::color;
  using ftxui::Decorator;
  using ftxui::inverted;

  return MenuEntryOption{
      .normal = Decorator(color(c)),
      .focused = Decorator(color(c)) | inverted,
      .selected = Decorator(color(c)) | inverted,
      .selected_focused = Decorator(color(c)) | inverted,
  };
}

/**
 * @brief Component to list files from given directory
 */
class ListDirectory : public Block {
  static constexpr int kMaxColumns = 30;     //!< Maximum columns for Component
  static constexpr int kMaxIconColumns = 2;  //!< Maximum columns for Icon

 public:
  /**
   * @brief Construct a new List Directory object
   * @param dispatcher Block event dispatcher
   * @param optional_path List files from custom path instead of the current one
   */
  explicit ListDirectory(const std::shared_ptr<EventDispatcher>& dispatcher,
                         const std::string& optional_path = "");

  /**
   * @brief Destroy the List Directory object
   */
  ~ListDirectory() override;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(ftxui::Event event) override;

  /**
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
 private:
  //! Handle mouse event
  bool OnMouseEvent(ftxui::Event event);

  //! Handle mouse wheel event
  bool OnMouseWheel(ftxui::Event event);

  //! Handle keyboard event mapped to a menu navigation command
  bool OnMenuNavigation(const ftxui::Event& event);

  //! Handle keyboard event when search mode is enabled
  bool OnSearchModeEvent(const ftxui::Event& event);

  /* ******************************************************************************************** */

  //! Getter for entries size
  int Size() const {
    return mode_search_ ? (int)mode_search_->entries.size() : (int)entries_.size();
  }

  //! Getter for selected index
  int* GetSelected() { return mode_search_ ? &mode_search_->selected : &selected_; }

  //! Getter for focused index
  int* GetFocused() { return mode_search_ ? &mode_search_->focused : &focused_; }

  //! Getter for entry at informed index
  util::File& GetEntry(int i) {
    return mode_search_ ? mode_search_->entries.at(i) : entries_.at(i);
  }

  //! Getter for active entry (focused/selected)
  util::File* GetActiveEntry() {
    if (!Size()) return nullptr;

    return mode_search_ ? &mode_search_->entries.at(mode_search_->selected)
                        : &entries_.at(selected_);
  }

  //! Clamp both selected and focused indexes
  void Clamp();

  //! Getter for Title (for testing purposes, may be overridden)
  virtual std::string GetTitle();

  /* ******************************************************************************************** */
  //! File list operations

  /**
   * @brief Refresh list with all files from the given directory path
   * @param dir_path Full path to directory
   * @return true if directory was parsed succesfully, false otherwise
   */
  bool RefreshList(const std::filesystem::path& dir_path);

  /**
   * @brief Refresh list to keep only files matching pattern from the text to search
   */
  void RefreshSearchList();

  /**
   * @brief Update content from active entry (decides if animation thread should run or not)
   */
  void UpdateActiveEntry();

  /**
   * @brief Select file to play based on the current song playing
   * @param pick_next Pick next or previous file to play
   * @return Filepath
   */
  util::File SelectFileToPlay(bool pick_next);

  /**
   * @brief Execute click action on active entry (may change directory or play song)
   * @return true if click action was executed, false otherwise
   */
  bool ClickOnActiveEntry();

  /* ******************************************************************************************** */
 protected:
  std::filesystem::path curr_dir_;                                    //!< Current directory
  std::optional<std::filesystem::path> curr_playing_ = std::nullopt;  //!< Current song playing

  //! Parameters for when search mode is enabled
  struct Search {
    std::string text_to_search;  //!< Text to search in file entries
    util::Files entries;  //!< List containing only files from current directory matching the text
    int selected;         //!< Entry index in files list for entry selected
    int focused;          //!< Entry index in files list for entry focused
    int position;         //!< Cursor position for text to search
  };

  //! Put together all possible styles for an entry in this component
  struct EntryStyles {
    ftxui::Decorator title;
    MenuEntryOption directory;
    MenuEntryOption file;
    MenuEntryOption playing;
  };

  /* ******************************************************************************************** */
  //! Custom class for text animation
 private:
  /**
   * @brief An structure to offset selected entry text when its content is too long (> 32 columns)
   */
  struct TextAnimation {
    std::mutex mutex;                  //!< Control access for internal resources
    std::condition_variable notifier;  //!< Conditional variable to block thread
    std::thread thread;                //!< Thread to perform offset animation on text

    std::atomic<bool> enabled = false;  //!< Flag to control thread animation
    std::string text;                   //!< Entry text to perform animation

    std::function<void()> cb_update;  //!< Force an UI refresh

    /**
     * @brief Start animation thread
     *
     * @param entry Text content from selected entry
     */
    void Start(const std::string& entry) {
      text = entry;
      enabled = true;

      thread = std::thread([this] {
        using namespace std::chrono_literals;
        std::unique_lock lock(mutex);

        // Run the animation every 0.2 seconds while enabled is true
        while (!notifier.wait_for(lock, 0.2s, [this] { return enabled == false; })) {
          // Here comes the magic
          text += text.front();
          text.erase(text.begin());

          // Notify UI
          cb_update();
        }
      });
    }

    /**
     * @brief Stop animation thread
     */
    void Stop() {
      if (enabled) {
        Notify();
        Exit();
      }
    }

   private:
    /**
     * @brief Disable thread execution
     */
    void Notify() {
      std::scoped_lock lock(mutex);
      enabled = false;
    }

    /**
     * @brief Notify thread and wait for its stop
     */
    void Exit() {
      notifier.notify_one();
      thread.join();
    }
  };

  /* ******************************************************************************************** */
  //! Variables

  util::Files entries_;  //!< List containing files from current directory
  int selected_;         //!< Entry index in files list for entry selected
  int focused_;          //!< Entry index in files list for entry focused

  std::vector<ftxui::Box> boxes_;  //!< Single box for each entry in files list
  ftxui::Box box_;                 //!< Box for whole component

  std::optional<Search> mode_search_ =
      std::nullopt;  //!< Mode to render only files matching the search pattern

  EntryStyles styles_ = EntryStyles{
      .title = ftxui::color(ftxui::Color::White) | ftxui::bold,
      .directory = Colored(ftxui::Color::Green),
      .file = Colored(ftxui::Color::White),
      .playing =
          Colored(ftxui::Color::SteelBlue1)};  //!< Style for each possible type of entry on menu

  TextAnimation animation_;  //!< Text animation for selected entry

  std::chrono::system_clock::time_point last_click_;  //!< Last timestamp that mouse was clicked

  util::FileHandler file_handler_;  //!< Utility class to list files

  /* ******************************************************************************************** */
  //! Friend test

#ifdef ENABLE_TESTS
  FRIEND_TEST(::ListDirectoryTest, RunTextAnimation);
  FRIEND_TEST(::ListDirectoryTest, ScrollMenuOnBigList);
  FRIEND_TEST(::ListDirectoryTest, TabMenuOnBigList);
  FRIEND_TEST(::ListDirectoryTest, PlayNextFileAfterFinished);
  FRIEND_TEST(::ListDirectoryTest, StartPlayingLastFileAndPlayNextAfterFinished);
  FRIEND_TEST(::ListDirectoryCtorTest, CreateWithBadInitialPath);
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_
