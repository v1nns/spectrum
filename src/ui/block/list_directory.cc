#include "ui/block/list_directory.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <utility>

#include "ui/colors.h"

namespace interface {

/* ********************************************************************************************** */

ListDirectory::ListDirectory(screen_portion_t init, screen_portion_t size)
    : Block(init, size, "Files", ListDirectory::InitialState::GetInstance()) {}

/* ********************************************************************************************** */

void ListDirectory::InitialState::Init(Block& block) {
  RefreshList(std::filesystem::current_path());
}

/* ********************************************************************************************** */

void ListDirectory::InitialState::Draw(Block& block) {
  auto window = block.GetWindow();
  int max_rows = getmaxy(window);

  // Use last row to show search box
  if (search_mode_) --max_rows;

  DrawCurrentDirectory(window);

  // Get items to print on screen
  const Items& to_print = GetItemsToPrint(max_rows);
  int row = 1;  // offset to not override the current directory text

  // Draw all filenames from this sub-list
  for (const auto& item : to_print) {
    DrawItem(window, row, item);
    if (++row == max_rows) break;
  }

  if (search_mode_) {
    DrawSearchBox(window, max_rows);
  }
};

/* ********************************************************************************************** */

void ListDirectory::InitialState::DrawCurrentDirectory(WINDOW* window) {
  // Show current directory (in bold) in the first row
  wattron(window, A_BOLD);
  mvwprintw(window, 0, 0, curr_dir_.c_str());
  wattroff(window, A_BOLD);
}

/* ********************************************************************************************** */

void ListDirectory::InitialState::DrawItem(WINDOW* window, int row, const Item& item) {
  if (item.is_hover) {
    wattron(window, A_STANDOUT);
  }

  if (item.is_dir) {
    wattron(window, COLOR_PAIR(kColorTextGreen));
  }

  mvwprintw(window, row, 1, item.path.c_str());

  wattroff(window, A_STANDOUT);
  wattroff(window, COLOR_PAIR(kColorTextGreen));
};

/* ********************************************************************************************** */

void ListDirectory::InitialState::DrawSearchBox(WINDOW* window, int max_rows) {
  wattron(window, A_DIM);
  mvwprintw(window, max_rows, 0, "Search:");
  wattroff(window, A_DIM);
  wprintw(window, text_to_search_.c_str());
}

/* ********************************************************************************************** */

void ListDirectory::InitialState::HandleInput(Block& block, int key) {
  // Consider that key input will be treated, otherwise "default" case will set to false
  bool refresh = true;

  switch (key) {
    /* ----------------------------------- Navigation commands ---------------------------------- */
    case KEY_DOWN: {
      auto active = GetActiveItem();
      if (std::next(active) != list_.end()) {
        active->is_hover = false;

        std::advance(active, 1);
        active->is_hover = true;
      }
      break;
    }

    case KEY_UP: {
      auto active = GetActiveItem();
      if (active != list_.begin()) {
        active->is_hover = false;

        std::advance(active, -1);
        active->is_hover = true;
      }
      break;
    }

    case KEY_HOME: {
      auto active = GetActiveItem();
      if (active != list_.begin()) {
        active->is_hover = false;

        active = list_.begin();
        active->is_hover = true;
      }
      break;
    }

    case KEY_END: {
      auto active = GetActiveItem();
      if (std::next(active) != list_.end()) {
        active->is_hover = false;

        active = std::prev(list_.end());
        active->is_hover = true;
      }
      break;
    }

    case KEY_ENTER:
    case 10:
    case 13: {
      if (!search_mode_) {
        std::filesystem::path new_path;
        auto active = GetActiveItem();

        if (active == list_.begin()) {
          if (std::filesystem::exists(curr_dir_.parent_path())) {
            // Okay, it really exists, so it should go back one level
            new_path = curr_dir_.parent_path();
          }
        } else if (active->is_dir) {
          // If active item is a directory, should enter it
          new_path = curr_dir_ / active->path;
        }

        if (!new_path.empty()) {
          RefreshList(new_path);
        }
      } else {
        // TODO: do something on search mode
      }
      break;
    }

    /* -------------------------------- Commands for Search Mode -------------------------------- */
    case KEY_BACKSPACE: {
      if (search_mode_ && text_to_search_.size() > 0) {
        text_to_search_.pop_back();
        UpdateTextToSearch();
      }
      break;
    }

    //! Keys 'Control+Backspace'
    case 8:
    case 23: {
      if (search_mode_) {
        text_to_search_.clear();
        UpdateTextToSearch();
      }
      break;
    }

    //! Key 'Esc'
    case 27: {
      if (search_mode_) {
        text_to_search_.clear();
        UpdateTextToSearch();

        search_mode_ = false;
      }
      break;
    }

    //! Key '/'
    case 47: {
      if (!search_mode_) {
        search_mode_ = true;
      } else {
        UpdateTextToSearch(key);
      }
      break;
    }

    default: {
      std::string dummy{keyname(key)};
      // Filter any other key sequence using 'Control'
      if (search_mode_ && dummy.at(0) != '^') {
        UpdateTextToSearch(key);
        break;
      }

      refresh = false;
    }
  }

  if (refresh) {
    block.ForceRefresh();
  }
}

/* ********************************************************************************************** */

ListDirectory::InitialState::ItemPtr ListDirectory::InitialState::GetActiveItem() {
  return std::find_if(list_.begin(), list_.end(), [](const Item& obj) { return obj.is_hover; });
}

/* ********************************************************************************************** */

ListDirectory::InitialState::Items ListDirectory::InitialState::GetItemsToPrint(int max_rows) {
  // Get active item (or highlighted) and get its distance from begin()
  auto active = GetActiveItem();
  long distance = std::distance(list_.begin(), active) + 1;  // taking into account the index zero

  // Calculate offset for sublist
  int offset = distance >= max_rows ? (distance - max_rows + 1) : 0;

  // Extract sub-list to be printed
  Items to_print{};
  ItemPtr it = list_.begin() + offset;

  int row = 0;
  while (it != list_.end() && row < max_rows) {
    if (!search_mode_ || (it->contains_search_term || text_to_search_.empty())) {
      to_print.emplace_back(*it);
      ++row;
    }

    ++it;
  }

  return to_print;
}

/* ********************************************************************************************** */

void ListDirectory::InitialState::RefreshList(const std::filesystem::path& dir_path) {
  curr_dir_ = dir_path;
  list_.clear();
  search_mode_ = false;

  // Add all dir/file from the current directory
  for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
    list_.emplace_back(Item{
        .path = entry.path().filename(),
        .is_dir = entry.is_directory(),
        .is_hover = false,
        .contains_search_term = false,
    });
  }

  // Transform whole string into uppercase
  auto to_lower = [](char& c) { c = std::tolower(c); };

  // Created a custom file sort
  auto custom_sort = [&to_lower](const Item& a, const Item& b) {
    std::string lhs{a.path}, rhs{b.path};

    // Don't care if it is hidden (tried to make it similar to "ls" output)
    if (lhs.at(0) == '.') lhs.erase(0, 1);
    if (rhs.at(0) == '.') rhs.erase(0, 1);

    std::for_each(lhs.begin(), lhs.end(), to_lower);
    std::for_each(rhs.begin(), rhs.end(), to_lower);

    return lhs < rhs;
  };

  // Sort list alphabetically (case insensitive)
  std::sort(list_.begin(), list_.end(), custom_sort);

  // Add option to go back one level
  list_.insert(list_.begin(), Item{
                                  .path = "..",
                                  .is_dir = true,
                                  .is_hover = true,
                                  .contains_search_term = false,
                              });
}

/* ********************************************************************************************** */

void ListDirectory::InitialState::UpdateTextToSearch(int key) {
  if (key != -1) text_to_search_.push_back(static_cast<char>(key));

  auto compare_string = [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); };
  bool replace_active_item = false;

  // Iterate through list to update items
  for (auto& item : list_) {
    if (search_mode_ && !text_to_search_.empty()) {
      auto it = std::search(item.path.begin(), item.path.end(), text_to_search_.begin(),
                            text_to_search_.end(), compare_string);

      item.contains_search_term = it != item.path.end();

      // In case it is the first matching entry, replace the current active item by this one
      if (item.contains_search_term && !replace_active_item) {
        auto active = GetActiveItem();
        active->is_hover = false;

        item.is_hover = true;
        replace_active_item = true;
      }

      continue;
    }

    // otherwise, just set to default value
    item.contains_search_term = false;
  }
}

}  // namespace interface
