/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_VIEW_BLOCK_LIST_DIRECTORY_H_
#define INCLUDE_VIEW_BLOCK_LIST_DIRECTORY_H_

#include <gtest/gtest_prod.h>

#include <atomic>
#include <condition_variable>
#include <filesystem>  // for path
#include <memory>      // for shared_ptr
#include <mutex>
#include <optional>  // for optional
#include <string>    // for string, allocator
#include <thread>
#include <vector>  // for vector

#include "ftxui/component/captured_mouse.hpp"     // for ftxui
#include "ftxui/component/component_options.hpp"  // for MenuEntryOption
#include "ftxui/dom/elements.hpp"                 // for Element
#include "ftxui/screen/box.hpp"                   // for Box
#include "view/base/block.h"                      // for Block, BlockEvent...

//! Forward declaration
namespace {
class ListDirectoryTest;
class ListDirectoryTest_RunTextAnimation_Test;
}  // namespace

namespace interface {

//! For better readability
using File = std::filesystem::path;  //!< Single file path
using Files = std::vector<File>;     //!< List of file paths

//! Custom style for menu entry
struct MenuEntryOption {
  ftxui::Decorator normal;
  ftxui::Decorator focused;
  ftxui::Decorator selected;
  ftxui::Decorator selected_focused;
};

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
  virtual ~ListDirectory();

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
  bool OnMenuNavigation(ftxui::Event event);

  //! Handle keyboard event when search mode is enabled
  bool OnSearchModeEvent(ftxui::Event event);

  /* ******************************************************************************************** */
 private:
  //! Getter for entries size
  int Size() const { return mode_search_ ? mode_search_->entries.size() : entries_.size(); }
  //! Getter for selected index
  int* GetSelected() { return mode_search_ ? &mode_search_->selected : &selected_; }
  //! Getter for focused index
  int* GetFocused() { return mode_search_ ? &mode_search_->focused : &focused_; }
  //! Getter for entry at informed index
  File& GetEntry(int i) { return mode_search_ ? mode_search_->entries.at(i) : entries_.at(i); }
  //! Getter for active entry (focused/selected)
  File* GetActiveEntry() {
    if (!Size()) return nullptr;

    return mode_search_ ? &mode_search_->entries.at(mode_search_->selected)
                        : &entries_.at(selected_);
  }

  //! Clamp both selected and focused indexes
  void Clamp();

  //! Getter for Title (for testing purposes, may be overriden)
  virtual std::string GetTitle();

  /* ******************************************************************************************** */
 private:
  /**
   * TODO: move this to a controller?
   * @brief Refresh list with all files from the given directory path
   * @param dir_path Full path to directory
   */
  void RefreshList(const std::filesystem::path& dir_path);

  /**
   * @brief Refresh list to keep only files matching pattern from the text to search
   */
  void RefreshSearchList();

  /**
   * @brief Update content from active entry (decides if animation thread should run or not)
   */
  void UpdateActiveEntry();

  /* ******************************************************************************************** */
 protected:
  std::filesystem::path curr_dir_;                     //!< Current directory
  std::optional<std::filesystem::path> curr_playing_;  //!< Current song playing

  //! Parameters for when search mode is enabled
  struct Search {
    std::string text_to_search;  //!< Text to search in file entries
    Files entries;          //!< List containing only files from current directory matching the text
    int selected, focused;  //!< Entry indexes in files list
    int position;           //!< Cursor position for text to search
  };

  //! Put together all possible styles for an entry in this component
  struct EntryStyles {
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

    std::atomic<bool> enabled;  //!< Flag to control thread animation
    std::string text;           //!< Entry text to perform animation

    std::function<void()> cb_update;  //!< Force an UI refresh

    /**
     * @brief Start animation thread
     *
     * @param entry Text content from selected entry
     */
    void Start(const std::string& entry) {
      text = entry;
      enabled = true;

      thread = std::thread([&] {
        using namespace std::chrono_literals;
        std::unique_lock<std::mutex> lock(mutex);

        // Run the animation every 0.2 seconds while enabled is true
        while (!notifier.wait_for(lock, 0.2s, [&] { return enabled == false; })) {
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
      {
        std::lock_guard<std::mutex> lock(mutex);
        enabled = false;
      }
      notifier.notify_one();
      thread.join();
    }
  };

  /* ******************************************************************************************** */
 private:
  Files entries_;           //!< List containing files from current directory
  int selected_, focused_;  //!< Entry indexes in files list

  std::vector<ftxui::Box> boxes_;  //!< Single box for each entry in files list
  ftxui::Box box_;                 //!< Box for whole component

  std::optional<Search> mode_search_;  //!< Mode to render only files matching the search pattern

  EntryStyles styles_;  //!< Style for each possible type of entry on menu

  TextAnimation animation_;  //!< Text animation for selected entry

  /* ******************************************************************************************** */
  //! Friend test
  FRIEND_TEST(::ListDirectoryTest, RunTextAnimation);
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_LIST_DIRECTORY_H_
