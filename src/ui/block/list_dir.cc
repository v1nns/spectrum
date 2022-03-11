#include "ui/block/list_dir.h"

#include <algorithm>
#include <filesystem>

namespace interface {

/* ********************************************************************************************** */

ListDir::ListDir(screen_portion_t init, screen_portion_t size)
    : Block(init, size, "Files", ListDir::InitialState::GetInstance()) {}

/* ********************************************************************************************** */

void ListDir::InitialState::Init(Block& block) {
  // TODO: move from here to something like COLORS header
  // Initialize color...
  init_pair(2, COLOR_GREEN, -1);

  RefreshList(std::filesystem::current_path());
}

/* ********************************************************************************************** */

// TODO: improve this method for when the block receives a KEY_DOWN/KEY_UP event
void ListDir::InitialState::Draw(Block& block) {
  auto window = block.GetWindow();
  int max_rows = getmaxy(window);
  //   werase(window);

  // Show current directory (in bold)
  wattron(window, A_BOLD);
  mvwprintw(window, 0, 0, curr_dir_.c_str());
  wattroff(window, A_BOLD);

  // Extract sub-list to be printed
  const int offset = 0;
  auto begin = list_.begin() + offset, end = list_.end() + offset;
  const std::vector<std::filesystem::path> to_print(begin, end);

  // Draw all filenames from this sub-list
  int row = 0;
  for (const auto& item : to_print) {
    DrawItem(window, row, item);
    if (++row == max_rows) break;
  }

  wnoutrefresh(window);
};

/* ********************************************************************************************** */

void ListDir::InitialState::DrawItem(WINDOW* window, int index, const std::filesystem::path& item) {
  if (highlighted_ == index) {
    wattron(window, A_STANDOUT);
  }

  if (std::filesystem::is_directory(item)) {
    wattron(window, COLOR_PAIR(2));
  }

  int index_offset = index + 1;  // do not override current directory
  mvwprintw(window, index_offset, 1, item.filename().c_str());

  wattroff(window, A_STANDOUT);
  wattroff(window, COLOR_PAIR(2));
};

/* ********************************************************************************************** */

void ListDir::InitialState::HandleInput(Block& block, int key) {
  bool refresh = false;
  switch (key) {
    // TODO: move window when new highlighted item exceeds visible window size
    case KEY_DOWN: {
      if (((size_t)highlighted_ + 1) < list_.size()) {
        ++highlighted_;
        refresh = true;
      }
      break;
    }

    case KEY_UP: {
      if ((highlighted_ - 1) > -1) {
        --highlighted_;
        refresh = true;
      }
      break;
    }

    case KEY_ENTER:
    case 10:
    case 13: {
      std::filesystem::path new_path;
      if (highlighted_ == 0) {
        if (std::filesystem::exists(curr_dir_.parent_path())) {
          // Okay, it really exists, so it should go back one level
          new_path = curr_dir_.parent_path();
        }
      } else if (std::filesystem::is_directory(list_.at(highlighted_))) {
        // If active item is a directory, should enter it
        new_path = curr_dir_ / list_.at(highlighted_);
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

void ListDir::InitialState::RefreshList(const std::filesystem::path& dir_path) {
  curr_dir_ = dir_path;
  highlighted_ = 0;
  list_.clear();

  // Add all dir/file from the current directory
  for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
    list_.emplace_back(entry);
  }

  // Transform whole string into uppercase
  auto to_upper = [](char& c) { c = std::toupper(c); };

  // Created a custom file sort
  auto custom_sort = [&to_upper](const std::filesystem::path& a, const std::filesystem::path& b) {
    std::string lhs = a.filename(), rhs = b.filename();

    // Don't care if it is hidden (similar to "ls" output)
    if (lhs.at(0) == '.') lhs.erase(0, 1);
    if (rhs.at(0) == '.') rhs.erase(0, 1);

    std::for_each(lhs.begin(), lhs.end(), to_upper);
    std::for_each(rhs.begin(), rhs.end(), to_upper);

    return lhs < rhs;
  };

  // Sort list alphabetically (case insensitive)
  std::sort(list_.begin(), list_.end(), custom_sort);

  // Add option to go back one level
  list_.insert(list_.begin(), std::filesystem::path(".."));
}

}  // namespace interface
