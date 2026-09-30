#include "view/element/internal/file_menu.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <string>
#include <string_view>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "util/file_handler.h"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/keybinding.h"

namespace interface {
namespace internal {

namespace {

using std::string_view_literals::operator""sv;

//! File extensions (in lowercase) considered as audio/media, so they are not dimmed on the list
constexpr std::array kMediaExtensions{
    // Audio
    ".aac"sv, ".ac3"sv, ".aif"sv, ".aifc"sv, ".aiff"sv, ".alac"sv, ".amr"sv, ".ape"sv, ".au"sv,
    ".caf"sv, ".dff"sv, ".dsf"sv, ".dts"sv, ".eac3"sv, ".flac"sv, ".m4a"sv, ".m4b"sv, ".mka"sv,
    ".mp2"sv, ".mp3"sv, ".mpc"sv, ".oga"sv, ".ogg"sv, ".opus"sv, ".ra"sv, ".tta"sv, ".wav"sv,
    ".weba"sv, ".wma"sv, ".wv"sv,
    // Video (usually containing an audio stream)
    ".3gp"sv, ".avi"sv, ".flv"sv, ".m4v"sv, ".mkv"sv, ".mov"sv, ".mp4"sv, ".mpeg"sv, ".mpg"sv,
    ".ogv"sv, ".ts"sv, ".webm"sv, ".wmv"sv};

//! Check if file extension belongs to a known audio/media format (it does not open the file)
bool HasMediaExtension(const util::File& file) {
  std::string extension = file.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  return std::find(kMediaExtensions.begin(), kMediaExtensions.end(), extension) !=
         kMediaExtensions.end();
}

}  // namespace

FileMenu::FileMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                   const std::shared_ptr<util::FileHandler>& file_handler,
                   const TextAnimation::Callback& force_refresh, const Callback& on_click,
                   const menu::Style& style, const std::string& optional_path)
    : BaseMenu(dispatcher, force_refresh), file_handler_{file_handler}, on_click_{on_click} {
  switch (style) {
    case menu::Style::Default:
      style_ = Style{
          .prefix = ftxui::color(ftxui::Color::SteelBlue1Bis),
          .directory = Colored(ftxui::Color::Green),
          .file = Colored(ftxui::Color::White),
          .playing = Colored(ftxui::Color::SteelBlue1),
      };
      break;

    case menu::Style::Alternative:
      style_ = Style{
          .prefix = ftxui::color(ftxui::Color::SteelBlue1Bis),
          .directory = Colored(ftxui::Color::DarkSeaGreen2Bis),
          .file = Colored(ftxui::Color::Grey11),
          .playing = Colored(ftxui::Color::SteelBlue1),
      };
      break;
  }

  // Files not looking like audio/media keep the same color, but dimmed
  style_.unsupported = MenuEntryOption{
      .normal = style_.file.normal | ftxui::dim,
      .focused = style_.file.focused | ftxui::dim,
      .selected = style_.file.selected | ftxui::dim,
      .selected_focused = style_.file.selected_focused | ftxui::dim,
  };

  auto filepath = ComposeDirectoryPath(optional_path);

  if (bool parsed = RefreshList(filepath); !optional_path.empty() && !parsed) {
    // If we can't list files from current path, then everything is gone
    RefreshList(std::filesystem::current_path());
  }
}

/* ********************************************************************************************** */

ftxui::Element FileMenu::RenderImpl() {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  auto max_size = GetMaxColumns() ? ftxui::size(WIDTH, EQUAL, GetMaxColumns()) : ftxui::nothing;

  ftxui::Elements menu_entries;

  int size = GetSize();
  menu_entries.reserve(size);

  const auto selected = GetSelected();
  const auto focused = GetFocused();
  auto& boxes = GetBoxes();

  // Fill list with entries
  for (int i = 0; i < size; ++i) {
    const auto& entry = IsSearchEnabled() ? filtered_entries_->at(i) : entries_.at(i);

    bool is_focused = (*focused == i);
    bool is_selected = (*selected == i);
    bool is_highlighted = highlighted_ && entry == *highlighted_;

    const auto& type = GetEntryStyle(entry, is_highlighted);

    auto prefix = ftxui::text(is_selected ? "▶ " : "  ");

    ftxui::Decorator style = is_selected ? (is_focused ? type.selected_focused : type.selected)
                                         : (is_focused ? type.focused : type.normal);

    auto focus_management = is_focused ? ftxui::select : ftxui::nothing;

    // In case of entry text too long, animation thread will be running, so we gotta take the
    // text content from there
    auto text = ftxui::text(IsAnimationRunning() && is_selected ? GetTextFromAnimation()
                                                                : entry.filename().string());

    menu_entries.push_back(ftxui::hbox({
                               prefix | style_.prefix,
                               text | style | ftxui::xflex,
                           }) |
                           max_size | focus_management | ftxui::reflect(boxes[i]));
  }

  // Let user know that search did not match anything
  if (IsSearchEnabled() && menu_entries.empty()) {
    menu_entries.push_back(RenderNoMatches());
  }

  ftxui::Elements content{
      ftxui::vbox(menu_entries) | ftxui::reflect(Box()) | ftxui::yframe | ftxui::flex,
  };

  // Append search box, if enabled
  if (IsSearchEnabled()) {
    content.push_back(RenderSearch());
  }

  return ftxui::vbox({
             ftxui::text(GetTitle()) | ftxui::color(ftxui::Color::White) | ftxui::bold,
             ftxui::vbox(content) | ftxui::flex,
         }) |
         ftxui::flex;
}

/* ********************************************************************************************** */

const FileMenu::MenuEntryOption& FileMenu::GetEntryStyle(const util::File& entry,
                                                         bool is_highlighted) const {
  if (is_highlighted) {
    return style_.playing;
  }
  if (std::filesystem::is_directory(entry)) {
    return style_.directory;
  }
  if (HasMediaExtension(entry)) {
    return style_.file;
  }

  return style_.unsupported;
}

/* ********************************************************************************************** */

bool FileMenu::OnEventImpl(const ftxui::Event& event) {
  // Enable search mode
  if (!IsSearchEnabled() && event == keybinding::Navigation::EnableSearch) {
    EnableSearch();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

int FileMenu::GetSizeImpl() const {
  int size = IsSearchEnabled() ? (int)filtered_entries_->size() : (int)entries_.size();
  return size;
}

/* ********************************************************************************************** */

std::string FileMenu::GetActiveEntryAsTextImpl() const {
  auto active = GetActiveEntryImpl();
  return active.has_value() ? active->filename().string() : "";
}

/* ********************************************************************************************** */

bool FileMenu::OnClickImpl() {
  auto active = GetActiveEntryImpl();

  if (!active.has_value()) return false;

  std::filesystem::path new_dir;
  bool going_up = false;

  if (active->filename() == ".." && std::filesystem::exists(curr_dir_.parent_path())) {
    // Change to parent folder
    new_dir = curr_dir_.parent_path();
    going_up = true;
  } else if (std::filesystem::is_directory(*active)) {
    // Change to selected folder
    new_dir = curr_dir_ / active->filename();
  }

  if (!new_dir.empty()) {
    // Search results belong to the current folder, so leave search mode before changing it
    ResetSearch();

    std::filesystem::path old_dir = curr_dir_;
    if (!RefreshList(new_dir)) return false;

    // When going to parent folder, select the folder we came from
    if (going_up) SelectEntryByFilename(old_dir.filename());

    return true;
  }

  // Otherwise, it is a file, so execute custom on_click function (implemented by owner class)
  return on_click_(*active);
}

/* ********************************************************************************************** */

void FileMenu::SelectEntryByFilename(const std::filesystem::path& filename) {
  auto it = std::find_if(entries_.begin(), entries_.end(),
                         [&filename](const util::File& f) { return f.filename() == filename; });

  if (it == entries_.end()) return;

  ResetState(static_cast<int>(it - entries_.begin()));
  UpdateActiveEntry();
}

/* ********************************************************************************************** */

void FileMenu::FilterEntriesBy(const std::string& text) {
  // Do not even try to find it in the main list
  if (text.empty()) {
    filtered_entries_ = entries_;
    return;
  }

  filtered_entries_->clear();

  // Filter entries
  for (const auto& entry : entries_) {
    if (util::contains(entry.filename().string(), text)) {
      filtered_entries_->push_back(entry);
    }
  }
}

/* ********************************************************************************************** */

std::filesystem::path FileMenu::ComposeDirectoryPath(const std::string& optional_path) {
  // By default, use current path from where spectrum was executed
  std::filesystem::path filepath = std::filesystem::current_path();

  if (optional_path.empty()) return filepath;

  // Remove last slash from given path
  std::string clean_path = optional_path.back() == '/'
                               ? optional_path.substr(0, optional_path.size() - 1)
                               : optional_path;

  // Check if given path is valid
  try {
    auto tmp = std::filesystem::canonical(clean_path);
    filepath = tmp;
  } catch (...) {
    ERROR("Invalid path, tried to compose canonical path using ", std::quoted(clean_path));
  }

  return filepath;
}

/* ********************************************************************************************** */

bool FileMenu::RefreshList(const std::filesystem::path& dir_path) {
  LOG("Refresh list with files from new directory=", std::quoted(dir_path.c_str()));
  util::Files tmp;

  if (!file_handler_->ListFiles(dir_path, tmp)) {
    auto dispatcher = GetDispatcher();
    if (!dispatcher) return false;

    dispatcher->SetApplicationError(error::kAccessDirFailed);

    return false;
  }

  LOG("Updating list with new entries, size=", tmp.size());

  // Reset internal values
  curr_dir_ = dir_path;
  SetEntries(tmp);  // Use this, because of the internal::Menu::Clamp logic

  return true;
}

/* ********************************************************************************************** */

bool FileMenu::Reload() {
  // Do not change list while user is searching on it
  if (IsSearchEnabled()) {
    return false;
  }

  util::Files tmp;

  if (!file_handler_->ListFiles(curr_dir_, tmp)) {
    ERROR("Cannot reload files from current directory=", std::quoted(curr_dir_.c_str()));
    return false;
  }

  // Nothing changed, so keep everything as it is
  if (tmp == entries_) {
    return false;
  }

  LOG("Reloading list with new entries, size=", tmp.size());
  auto active = GetActiveEntryImpl();

  SetEntries(tmp);

  // Keep the same entry selected
  if (active.has_value()) {
    SelectEntryByFilename(active->filename());
  }

  return true;
}

/* ********************************************************************************************** */

std::string FileMenu::GetTitle() const {
#ifdef ENABLE_TESTS
  // It means it is running tests, so we always show only the directory name
  return curr_dir_.filename().string();
#endif

  const std::string curr_dir = curr_dir_.string();
  int max_columns = GetMaxColumns();

  // Everything fine, directory does not exceed maximum column length
  if (curr_dir.size() <= max_columns) {
    return curr_dir;
  }

  // Oh no, it does exceed, so we must truncate the exceeding text
  int offset =
      (int)curr_dir.size() - (max_columns - 5);  // Considering window border(2) + ellipsis(3)
  const std::string& substr = curr_dir.substr(offset);
  auto index = substr.find('/');

  // TODO: implement logic for when the dirname exceeds the max_columns by itself

  return index != std::string::npos ? std::string("..." + substr.substr(index)) : substr;
}

/* ********************************************************************************************** */

void FileMenu::SetEntriesImpl(const util::Files& entries) {
  LOG("Set a new list of entries with size=", entries.size());
  entries_ = entries;
}

/* ********************************************************************************************** */

void FileMenu::EmplaceImpl(const util::File& entry) {
  LOG("Emplace a new entry to list");
  entries_.emplace_back(entry);
}

/* ********************************************************************************************** */

void FileMenu::EraseImpl(const util::File& entry) {
  LOG("Attempt to erase an entry with value=", entry);
  auto it = std::find_if(entries_.begin(), entries_.end(),
                         [&entry](const util::File& f) { return f == entry; });

  if (it != entries_.end()) {
    LOG("Found matching entry, erasing it, entry=", *it);
    entries_.erase(it);
  }
}

/* ********************************************************************************************** */

bool FileMenu::SetEntryHighlightedImpl(const util::File& entry) {
  // Find entry in internal list
  auto it = std::find(entries_.begin(), entries_.end(), entry);

  if (it == entries_.end()) {
    LOG("Could not find entry to highlight");
    return false;
  }

  highlighted_ = *it;

  // To get a better experience, update focused and select indexes,
  // to highlight current playing song entry in list
  int index = static_cast<int>(it - entries_.begin());

  ResetState(index);

  return true;
}

/* ********************************************************************************************** */

std::optional<util::File> FileMenu::GetActiveEntryImpl() const {
  int size = GetSizeImpl();

  // Empty list
  if (!size) return std::nullopt;

  // Get active entry
  std::optional<util::File> entry = std::nullopt;
  int index = GetSelected();

  // Check for boundary and if vector not empty
  if (index >= size || entries_.empty() ||
      (filtered_entries_.has_value() && filtered_entries_->empty()))
    return entry;

  entry = IsSearchEnabled() ? filtered_entries_->at(index) : entries_.at(index);
  return entry;
}

}  // namespace internal
}  // namespace interface
