#include "ui/block/list_dir.h"

#include <algorithm>
#include <filesystem>

namespace interface {

/* ********************************************************************************************** */

ListDir::ListDir(screen_portion_t init, screen_portion_t size)
    : Block(init, size, "Files", ListDir::InitialState::GetInstance()) {}

/* ********************************************************************************************** */

void ListDir::InitialState::Init(Block& block) { RefreshList(); }

/* ********************************************************************************************** */

void ListDir::InitialState::Draw(Block& block) {
  auto window = block.GetWindow();
  werase(window);

  // Show current directory (in bold)
  wattron(window, A_UNDERLINE);
  mvwprintw(window, 0, 0, curr_dir_.c_str());
  wattroff(window, A_UNDERLINE);

  // List files
  int row = 1;
  for (const auto& file : files_) {
    if ((highlighted_ + 1) == row) {
      wattron(window, A_STANDOUT);
    }

    mvwprintw(window, row, 1, file.c_str());

    wattroff(window, A_STANDOUT);
    ++row;
  }

  wrefresh(window);
};

/* ********************************************************************************************** */

void ListDir::InitialState::HandleInput(Block& block, int key) {
  bool refresh = false;
  switch (key) {
    case KEY_DOWN: {
      if (((size_t)highlighted_ + 1) < files_.size()) {
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
    default:
      break;
  }

  if (refresh) {
    block.ForceRefresh();
  }
}

/* ********************************************************************************************** */

void ListDir::InitialState::RefreshList() {
  std::filesystem::path cwd = std::filesystem::current_path();
  curr_dir_ = cwd;
  highlighted_ = 0;

  for (const auto& entry : std::filesystem::directory_iterator(cwd)) {
    files_.emplace_back(entry.path().filename());
  }

  // Sort list alphabetically (case insensitive)
  std::sort(files_.begin(), files_.end(), [](std::string a, std::string b) {
    std::for_each(a.begin(), a.end(), [](char& c) { c = ::toupper(c); });
    std::for_each(b.begin(), b.end(), [](char& c) { c = ::toupper(c); });
    return a < b;
  });
}

}  // namespace interface
