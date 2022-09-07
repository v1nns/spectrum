/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_VIEW_BLOCK_LIST_DIRECTORY_H_
#define INCLUDE_VIEW_BLOCK_LIST_DIRECTORY_H_

#include <atomic>
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

namespace interface {

//! For better readability
using File = std::filesystem::path;  //!< Single file path
using Files = std::vector<File>;     //!< List of file paths

/**
 * @brief Component to list files from given directory
 */
class ListDirectory : public Block {
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
   * @brief Refresh list with all files from the given directory path TODO: move this to a
   * controller?
   * @param dir_path Full path to directory
   */
  void RefreshList(const std::filesystem::path& dir_path);

  /**
   * @brief Refresh list to keep only files matching pattern from the text to search
   */
  void RefreshSearchList();

  /* ******************************************************************************************** */
 protected:
  std::filesystem::path curr_dir_;                     //!< Current directory
  std::optional<std::filesystem::path> curr_playing_;  //!< Current song playing

  //! Parameters for when search mode is enabled
  struct Search {
    std::string text_to_search;  //!< Text to search in file entries
    Files entries;          //!< List containing only files from current directory matching the text
    int selected, focused;  //!< Entry indexes in files list
  };

  //! Put together all possible styles for an entry in this component
  struct EntryStyles {
    ftxui::MenuEntryOption directory;
    ftxui::MenuEntryOption file;
    ftxui::MenuEntryOption playing;
  };

  /* ******************************************************************************************** */
  //! Custom class for text animation
 private:
  /**
   * @brief An structure to offset selected entry text when its content is too long (> 32 columns)
   */
  struct TextAnimation {
    std::mutex mutex;    //!< Control access for internal resources
    std::thread thread;  //!< Thread to perform offset animation on text

    std::atomic<bool> enabled = false;  //!< Flag to control thread animation
    std::string text;                   //!< Entry text with an offset

    std::function<void()> cb_update;  //!< Force an UI refresh

    /**
     * @brief Start animation thread
     *
     * @param entry Text content from selected entry
     */
    void Start(std::string entry) {
      text = entry;
      enabled = true;

      thread = std::thread([&] {
        while (enabled) {
          using namespace std::chrono_literals;
          // TODO: remove this sleep and use something like a condition_variable, to wake up as soon
          // some event is received
          std::this_thread::sleep_for(0.15s);
          {
            std::unique_lock<std::mutex> lock(mutex);

            // Here comes the magic
            text += text.front();
            text.erase(text.begin());

            cb_update();
          }
        }
      });
    }

    /**
     * @brief Stop animation thread
     */
    void Stop() {
      enabled = false;
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
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_LIST_DIRECTORY_H_
