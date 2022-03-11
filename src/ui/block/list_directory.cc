#include "ui/block/list_directory.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <utility>

namespace interface {

/* ********************************************************************************************** */

ListDirectory::ListDirectory(screen_portion_t init, screen_portion_t size)
    : Block(init, size, "Files", ListDirectory::InitialState::GetInstance()) {}

/* ********************************************************************************************** */

void ListDirectory::InitialState::Init(Block& block) {
  // TODO: move from here to something like COLORS header
  // Initialize color...
  init_pair(2, COLOR_GREEN, -1);

  RefreshList(std::filesystem::current_path());
}

/* ********************************************************************************************** */

void ListDirectory::InitialState::Draw(Block& block) {
  auto window = block.GetWindow();
  int max_rows = getmaxy(window);
  werase(window);

  // Show current directory (in bold)
  wattron(window, A_BOLD);
  mvwprintw(window, 0, 0, curr_dir_.c_str());
  wattroff(window, A_BOLD);

  // Get active item (or highlighted) and get its distance from begin()
  auto active = GetItemHighlighted();
  long distance = std::distance(list_.begin(), active) + 1;  // taking into account the index zero

  // Calculate offset for sublist
  int offset = distance >= max_rows ? (distance - max_rows + 1) : 0;

  // Extract sub-list to be printed
  // TODO: improve end() to consider only the offset
  auto begin = list_.begin() + offset, end = list_.end();
  const std::vector<Item> to_print(begin, end);

  // Draw all filenames from this sub-list
  int row = 1;  // offset to not override the current directory text
  for (const auto& item : to_print) {
    DrawItem(window, row, item);
    if (++row == max_rows) break;
  }

  wnoutrefresh(window);
};

/* ********************************************************************************************** */

void ListDirectory::InitialState::DrawItem(WINDOW* window, int row, const Item& item) {
  if (item.is_highlighted) {
    wattron(window, A_STANDOUT);
  }

  if (item.is_dir) {
    wattron(window, COLOR_PAIR(2));
  }

  mvwprintw(window, row, 1, item.path.c_str());

  wattroff(window, A_STANDOUT);
  wattroff(window, COLOR_PAIR(2));
};

/* ********************************************************************************************** */

void ListDirectory::InitialState::HandleInput(Block& block, int key) {
  bool refresh = false;
  switch (key) {
    case KEY_DOWN: {
      auto active = GetItemHighlighted();
      if (std::next(active) != list_.end()) {
        active->is_highlighted = false;

        std::advance(active, 1);
        active->is_highlighted = true;

        refresh = true;
      }
      break;
    }

    case KEY_UP: {
      auto active = GetItemHighlighted();
      if (active != list_.begin()) {
        active->is_highlighted = false;

        std::advance(active, -1);
        active->is_highlighted = true;

        refresh = true;
      }
      break;
    }

    case KEY_HOME: {
      auto active = GetItemHighlighted();
      if (active != list_.begin()) {
        active->is_highlighted = false;

        active = list_.begin();
        active->is_highlighted = true;

        refresh = true;
      }
      break;
    }

    case KEY_END: {
      auto active = GetItemHighlighted();
      if (std::next(active) != list_.end()) {
        active->is_highlighted = false;

        active = std::prev(list_.end());
        active->is_highlighted = true;

        refresh = true;
      }
      break;
    }

    case KEY_ENTER:
    case 10:
    case 13: {
      std::filesystem::path new_path;
      auto active = GetItemHighlighted();

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
        refresh = true;
      }
      break;
    }

    default:
      break;
  }

  if (refresh) {
    block.ForceRefresh();
  }
}

/* ********************************************************************************************** */

void ListDirectory::InitialState::RefreshList(const std::filesystem::path& dir_path) {
  curr_dir_ = dir_path;
  list_.clear();

  // Add all dir/file from the current directory
  for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
    list_.emplace_back(Item{
        .path = entry.path().filename(),
        .is_dir = entry.is_directory(),
        .is_highlighted = false,
    });
  }

  // Transform whole string into uppercase
  auto to_upper = [](char& c) { c = std::toupper(c); };

  // Created a custom file sort
  auto custom_sort = [&to_upper](const Item& a, const Item& b) {
    std::string lhs{a.path}, rhs{b.path};

    // Don't care if it is hidden (tried to make it similar to "ls" output)
    if (lhs.at(0) == '.') lhs.erase(0, 1);
    if (rhs.at(0) == '.') rhs.erase(0, 1);

    std::for_each(lhs.begin(), lhs.end(), to_upper);
    std::for_each(rhs.begin(), rhs.end(), to_upper);

    return lhs < rhs;
  };

  // Sort list alphabetically (case insensitive)
  std::sort(list_.begin(), list_.end(), custom_sort);

  // Add option to go back one level
  list_.insert(list_.begin(), Item{.path = "..", .is_dir = true, .is_highlighted = true});
}

/* ********************************************************************************************** */

std::vector<ListDirectory::InitialState::Item>::iterator
ListDirectory::InitialState::GetItemHighlighted() {
  return std::find_if(list_.begin(), list_.end(),
                      [](const Item& obj) { return obj.is_highlighted; });
}

}  // namespace interface
