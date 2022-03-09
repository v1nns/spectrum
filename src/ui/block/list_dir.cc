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
  werase(window);

  // Show current directory (in bold)
  wattron(window, A_BOLD);
  mvwprintw(window, 0, 0, curr_dir_.c_str());
  wattroff(window, A_BOLD);

  // List files
  int row = 1;
  for (const auto& item : list_) {
    if ((highlighted_ + 1) == row) {
      wattron(window, A_STANDOUT);
    }

    if (std::filesystem::is_directory(item)) {
      wattron(window, COLOR_PAIR(2));
    }

    mvwprintw(window, row, 1, item.filename().c_str());

    wattroff(window, A_STANDOUT);
    wattroff(window, COLOR_PAIR(2));
    ++row;
  }

  wrefresh(window);
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

  // Add option to go back one level
  list_.emplace_back(std::filesystem::path(".."));

  // Add all dir/file from the current directory
  for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
    list_.emplace_back(entry);
  }

  // Sort list alphabetically (case insensitive)
  std::sort(list_.begin(), list_.end(), [](std::filesystem::path a, std::filesystem::path b) {
    std::string dummy_a = a.filename(), dummy_b = b.filename();

    std::for_each(dummy_a.begin(), dummy_a.end(), [](char& c) { c = ::toupper(c); });
    std::for_each(dummy_b.begin(), dummy_b.end(), [](char& c) { c = ::toupper(c); });

    return dummy_a < dummy_b;
  });
}

}  // namespace interface
