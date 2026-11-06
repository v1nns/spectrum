#include "view/block/main_content/listening_statistics.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <tuple>
#include <utility>

#include "util/logger.h"
#include "view/element/style.h"
#include "view/element/util.h"

namespace interface {

namespace {

//! Get current moment (seconds since epoch)
int64_t Now() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

/**
 * @brief Node that keeps the whole box from its child, even the part not visible inside a frame
 * (unlike ftxui::reflect, which keeps only the visible part)
 */
class WholeBox : public ftxui::Node {
 public:
  WholeBox(ftxui::Element child, ftxui::Box& box) : ftxui::Node({std::move(child)}), box_{box} {}

  void SetBox(ftxui::Box box) override {
    box_ = box;
    ftxui::Node::SetBox(box);
    children_[0]->SetBox(box);
  }

 private:
  ftxui::Box& box_;
};

//! Same as ftxui::reflect, but keeping the whole box (even the part not visible)
ftxui::Decorator whole_box(ftxui::Box& box) {
  return [&box](ftxui::Element child) { return std::make_shared<WholeBox>(std::move(child), box); };
}

//! Format time listened, using only its two biggest units (e.g. "3 d 4 h", "12 min 5 s")
std::string FormatDuration(uint64_t ms) {
  constexpr uint64_t kSecond = 1000;
  constexpr uint64_t kMinute = 60 * kSecond;
  constexpr uint64_t kHour = 60 * kMinute;
  constexpr uint64_t kDay = 24 * kHour;

  struct Unit {
    uint64_t size;
    const char* name;
  };

  constexpr std::array<Unit, 4> kUnits{
      {{kDay, "d"}, {kHour, "h"}, {kMinute, "min"}, {kSecond, "s"}}};

  for (std::size_t i = 0; i < kUnits.size(); i++) {
    if (ms < kUnits[i].size && i + 1 < kUnits.size()) continue;

    std::string text = std::to_string(ms / kUnits[i].size) + " " + kUnits[i].name;

    if (uint64_t rest = i + 1 < kUnits.size() ? (ms % kUnits[i].size) / kUnits[i + 1].size : 0;
        rest > 0) {
      text += " " + std::to_string(rest) + " " + kUnits[i + 1].name;
    }

    return text;
  }

  return "0 s";
}

//! Format data size (e.g. "8.4 GB")
std::string FormatBytes(uint64_t bytes) {
  constexpr double kBase = 1000.;
  constexpr std::array<const char*, 5> kUnits{"B", "KB", "MB", "GB", "TB"};

  auto value = static_cast<double>(bytes);
  std::size_t unit = 0;

  while (value >= kBase && unit + 1 < kUnits.size()) {
    value /= kBase;
    unit++;
  }

  std::ostringstream ss;
  ss << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << " " << kUnits[unit];
  return std::move(ss).str();
}

//! Convert moment (seconds since epoch) to local time
std::tm ToLocalTime(int64_t seconds) {
  const auto moment = static_cast<std::time_t>(seconds);
  std::tm local{};
  localtime_r(&moment, &local);
  return local;
}

//! Format date from moment in local time (e.g. "Nov 5, 2026", with day not padded)
std::string FormatDate(int64_t seconds) {
  const std::tm local = ToLocalTime(seconds);

  std::ostringstream ss;
  ss << std::put_time(&local, "%b ") << local.tm_mday << std::put_time(&local, ", %Y");
  return std::move(ss).str();
}

//! Format number of days (e.g. "1 day", "12 days")
std::string FormatDays(uint32_t days) {
  return std::to_string(days) + (days == 1 ? " day" : " days");
}

//! Format time of day from moment in local time (e.g. "22:41")
std::string FormatTimeOfDay(int64_t seconds) {
  const std::tm local = ToLocalTime(seconds);

  std::ostringstream ss;
  ss << std::put_time(&local, "%H:%M");
  return std::move(ss).str();
}

}  // namespace

/* ********************************************************************************************** */

ListeningStatistics::ListeningStatistics(const model::BlockIdentifier& id,
                                         const std::shared_ptr<EventDispatcher>& dispatcher,
                                         const FocusCallback& on_focus,
                                         const keybinding::Key& keybinding,
                                         const std::shared_ptr<util::FileHandler>& file_handler)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string{kTabName}),
      file_handler_{file_handler},
      session_start_{Now()} {
  // Missing (or invalid) file means that nothing was listened to yet
  if (file_handler_ && !file_handler_->ParseStats(all_time_)) all_time_ = model::ListeningStats{};
  saved_ = all_time_;
}

/* ********************************************************************************************** */

ListeningStatistics::~ListeningStatistics() { Save(); }

/* ********************************************************************************************** */

ftxui::Element ListeningStatistics::Render() {
  const auto& theme = GetTheme().file_info;
  const model::ListeningStats& stats = show_all_time_ ? all_time_ : session_;

  ftxui::Elements lines{DrawScope(), ftxui::text("")};

  if (stats.IsEmpty()) {
    lines.push_back(ftxui::text("Nothing listened to yet") | ftxui::color(theme.value_empty));
    lines.push_back(ftxui::text("Songs played show up here") | ftxui::color(theme.field));
  } else {
    // Hours listened go on the right of totals, or below them when there is no room for both
    lines.push_back(ftxui::flexbox({DrawTotals(stats), DrawHours(stats)},
                                   ftxui::FlexboxConfig().SetGap(kColumnGap, 1)));
    lines.push_back(ftxui::text(""));
    lines.push_back(DrawTopLists(stats));
  }

  // Frame keeps focused row on its center, so it is placed half view below the first row to show
  const int focused_row = scroll_ + (view_box_.y_max - view_box_.y_min) / 2;

  // Margin on both sides, as content starts right after block border
  return ftxui::hbox({
             ftxui::text(" "),
             ftxui::vbox(std::move(lines)) | ftxui::xflex | whole_box(content_box_),
             ftxui::text(" "),
         }) |
         ftxui::focusPosition(0, focused_row) | ftxui::vscroll_indicator | ftxui::yframe |
         ftxui::reflect(view_box_);
}

/* ********************************************************************************************** */

bool ListeningStatistics::OnEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  if (event == keybinding::Stats::ToggleScope) {
    show_all_time_ = !show_all_time_;
    LOG("Handle key to show stats from ", show_all_time_ ? "all time" : "this session");
    return true;
  }

  if (event == Keybind::ArrowUp || event == Keybind::Up) return Scroll(-1);
  if (event == Keybind::ArrowDown || event == Keybind::Down) return Scroll(1);

  return false;
}

/* ********************************************************************************************** */

bool ListeningStatistics::OnMouseEvent(ftxui::Event& event) {
  const auto& mouse = event.mouse();

  if (const bool wheel_up = mouse.button == ftxui::Mouse::WheelUp;
      (wheel_up || mouse.button == ftxui::Mouse::WheelDown) &&
      view_box_.Contain(mouse.x, mouse.y)) {
    // Set focus on parent block, so keys go to stats after scrolling them
    if (on_focus_) on_focus_();

    Scroll(wheel_up ? -1 : 1);
    return true;
  }

  if (mouse.button != ftxui::Mouse::Left || mouse.motion != ftxui::Mouse::Released) return false;

  if (all_time_box_.Contain(mouse.x, mouse.y)) {
    show_all_time_ = true;
    return true;
  }

  if (session_box_.Contain(mouse.x, mouse.y)) {
    show_all_time_ = false;
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool ListeningStatistics::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    const auto song = event.GetContent<model::Song>();
    const bool from_url = song.stream_info.has_value();
    const std::string key = from_url ? song.stream_info->base_url : song.filepath.string();

    // Without a title, use the same as UI to identify song
    const std::string title = !song.title.empty() ? song.title
                              : from_url          ? song.stream_info->base_url
                                                  : song.filepath.filename().string();

    // Song is only counted once it is listened to
    current_ = Current{.key = key, .artist = song.artist, .title = title, .from_url = from_url};
    last_position_.reset();
  }

  if (event == CustomEvent::Identifier::UpdateSongState && current_) {
    const auto info = event.GetContent<model::Song::CurrentInformation>();

    // Only small steps forward are counted (seek, or position going back a little after stream is
    // opened again, are not something listened to)
    if (info.state == model::Song::MediaState::Play && last_position_ &&
        info.position_ms > *last_position_ &&
        info.position_ms - *last_position_ <= kMaxPositionStep) {
      const uint64_t listened = info.position_ms - *last_position_;
      const Current& song = *current_;
      const int64_t now = Now();

      Update([&](model::ListeningStats& stats) {
        stats.AddListened(song.key, song.artist, song.title, song.from_url, listened, now);
      });

      current_->listened_ms += listened;
      unsaved_ms_ += listened;
    }

    // Song skipped right after starting is not counted as played (but it was skipped)
    const bool finished = info.state == model::Song::MediaState::Finished;

    if (!current_->played && (finished || current_->listened_ms >= kPlayedThreshold)) {
      current_->played = true;
      const int64_t now = Now();
      Update([this, now](model::ListeningStats& stats) { stats.AddPlayed(current_->key, now); });
    }

    if (finished && !current_->finished) {
      current_->finished = true;
      Update([](model::ListeningStats& stats) { stats.songs_finished++; });
    }

    // Otherwise, a long song (or application closed abruptly) would lose a lot of what was counted
    if (unsaved_ms_ >= kSaveInterval) Save();

    last_position_ = info.position_ms;
  }

  if (event == CustomEvent::Identifier::ClearSongInfo && current_) {
    // Song stopped before its end (skipped, stopped by user, another song chosen or some error)
    if (!current_->finished) {
      Update([](model::ListeningStats& stats) { stats.songs_skipped++; });
    }

    current_.reset();
    last_position_.reset();
    Save();
  }

  if (event == CustomEvent::Identifier::UpdateStreamUsage) {
    const auto bytes = event.GetContent<uint64_t>();
    Update([bytes](model::ListeningStats& stats) { stats.stream_bytes += bytes; });
  }

  // Other blocks may also use these events
  return false;
}

/* ********************************************************************************************** */

void ListeningStatistics::Save() {
  unsaved_ms_ = 0;
  if (!file_handler_ || all_time_ == saved_) return;

  // Another instance may have saved stats in the meantime (a missing file means that they were
  // removed by user, so only what was counted since then is kept)
  model::ListeningStats latest;
  if (!file_handler_->ParseStats(latest)) latest = model::ListeningStats{};

  latest.Add(all_time_.Difference(saved_));

  if (!file_handler_->SaveStats(latest)) {
    ERROR("Cannot save listening stats");
    return;
  }

  all_time_ = latest;
  saved_ = std::move(latest);
}

/* ********************************************************************************************** */

bool ListeningStatistics::Scroll(int rows) {
  // Content taller than view can be scrolled until its last row is shown
  const int max_scroll =
      std::max(0, (content_box_.y_max - content_box_.y_min) - (view_box_.y_max - view_box_.y_min));

  const int scroll = std::clamp(scroll_ + rows, 0, max_scroll);
  if (scroll == scroll_) return false;

  scroll_ = scroll;
  return true;
}

/* ********************************************************************************************** */

ftxui::Element ListeningStatistics::DrawScope() {
  const auto& theme = GetTheme().file_info;

  auto option = [&theme](const std::string& text, bool selected, ftxui::Box& box) {
    ftxui::Element element = ftxui::text(text);
    element = selected ? element | ftxui::bold | ftxui::underlined | ftxui::color(theme.title)
                       : element | ftxui::color(theme.field);
    return element | ftxui::reflect(box);
  };

  return ftxui::hbox({
      option("all time", show_all_time_, all_time_box_),
      ftxui::text("  "),
      option("this session", !show_all_time_, session_box_),
  });
}

/* ********************************************************************************************** */

ftxui::Element ListeningStatistics::DrawField(const std::string& field,
                                              const std::string& value) const {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  const auto& theme = GetTheme().file_info;

  return ftxui::hbox({
      ftxui::text(field) | ftxui::size(WIDTH, EQUAL, kFieldColumns) | ftxui::color(theme.field),
      ftxui::text(value) | ftxui::color(theme.value),
  });
}

/* ********************************************************************************************** */

ftxui::Element ListeningStatistics::DrawTotals(const model::ListeningStats& stats) const {
  // Session stats start when application starts, even if no song was played right away
  const std::string since =
      show_all_time_ ? FormatDate(stats.since) : FormatTimeOfDay(session_start_);

  std::string skipped = std::to_string(stats.songs_skipped);

  // Rate considers only songs that already stopped (not the one playing right now)
  if (uint32_t stopped = stats.songs_finished + stats.songs_skipped; stopped > 0) {
    constexpr uint32_t kPercent = 100;
    skipped +=
        " (" + std::to_string((stats.songs_skipped * kPercent + stopped / 2) / stopped) + "%)";
  }

  ftxui::Elements lines{
      DrawField("Listening since", since),
      DrawField("Time listened", FormatDuration(stats.listened_ms)),
      DrawField("Songs played", std::to_string(stats.songs_played)),
      DrawField("Songs finished", std::to_string(stats.songs_finished)),
      DrawField("Songs skipped", skipped),
      DrawField("Local files", FormatDuration(stats.local_ms)),
      DrawField("YouTube", FormatDuration(stats.stream_ms)),
      DrawField("Audio streamed", FormatBytes(stats.stream_bytes)),
  };

  return ftxui::vbox(std::move(lines));
}

/* ********************************************************************************************** */

ftxui::Element ListeningStatistics::DrawHours(const model::ListeningStats& stats) const {
  using Stats = model::ListeningStats;

  constexpr std::array<const char*, Stats::kWeekdays> kWeekdayNames{"Mon", "Tue", "Wed", "Thu",
                                                                    "Fri", "Sat", "Sun"};

  // Each shade goes with a color from gradient (except the first one, for hours not listened)
  constexpr std::array<const char*, 5> kShades{"·", "░", "▒", "▓", "█"};
  static_assert(kShades.size() - 1 == std::tuple_size_v<decltype(Theme::Visualizer::gradient)>);

  const auto& theme = GetTheme();

  // Weekday name and a space before its hours
  const std::size_t label_columns = std::string_view{kWeekdayNames.front()}.size() + 1;

  ftxui::Elements lines{ftxui::text("Listening hours") | ftxui::bold |
                        ftxui::color(theme.file_info.title)};

  uint64_t busiest = 0;
  for (const auto& day : stats.hours) {
    busiest = std::max(busiest, *std::max_element(day.begin(), day.end()));
  }

  // Stats saved before hours were counted
  if (busiest == 0) {
    lines.push_back(ftxui::text("Counted from now on") | ftxui::color(theme.file_info.field));
    return ftxui::vbox(std::move(lines));
  }

  // Shade is relative to the hour listened the most (any time listened gets at least the lightest)
  const auto levels = static_cast<uint64_t>(kShades.size() - 1);

  for (std::size_t day = 0; day < Stats::kWeekdays; day++) {
    ftxui::Elements cells{ftxui::text(std::string{kWeekdayNames[day]} + " ") |
                          ftxui::color(theme.file_info.field)};

    for (uint64_t listened : stats.hours[day]) {
      const auto level = static_cast<std::size_t>((listened * levels + busiest - 1) / busiest);

      ftxui::Color color = theme.file_info.field;
      if (level > 0) {
        const auto& stop = theme.visualizer.gradient[level - 1];
        color = theme.visualizer.solid.value_or(ftxui::Color(stop.red, stop.green, stop.blue));
      }

      cells.push_back(ftxui::text(kShades[level]) | ftxui::color(color));
    }

    lines.push_back(ftxui::hbox(std::move(cells)));
  }

  // Hour labels are placed right below their column
  std::string axis(label_columns + Stats::kDayHours, ' ');

  for (std::size_t hour = 0; hour < Stats::kDayHours; hour += kHourLabelStep) {
    const std::string label = std::to_string(hour);
    axis.replace(label_columns + hour, label.size(), label);
  }

  lines.push_back(ftxui::text(axis) | ftxui::color(theme.file_info.field));

  // Streak is about days, so it is not shown for a single session (nor before any song is played)
  if (show_all_time_ && stats.streak.longest > 0) {
    lines.push_back(ftxui::hbox({
        ftxui::text("Streak  ") | ftxui::color(theme.file_info.field),
        ftxui::text(FormatDays(stats.GetCurrentStreak(Now())) + " · longest " +
                    std::to_string(stats.streak.longest)) |
            ftxui::color(theme.file_info.value),
    }));
  }

  return ftxui::vbox(std::move(lines));
}

/* ********************************************************************************************** */

ftxui::Element ListeningStatistics::DrawTopLists(const model::ListeningStats& stats) const {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  const auto& theme = GetTheme().file_info;

  // Name is cut when there is not enough space for it, so time listened is aligned on both lists
  auto entry = [&theme](std::size_t index, const std::string& name, uint64_t listened_ms) {
    return ftxui::hbox({
        ftxui::text(std::to_string(index + 1) + ". ") | ftxui::color(theme.field),
        ellipsized_text(name, 0) | ftxui::xflex | ftxui::color(theme.value),
        ftxui::text(" "),
        ftxui::text(FormatDuration(listened_ms)) | ftxui::align_right |
            ftxui::size(WIDTH, EQUAL, kTimeColumns) | ftxui::color(theme.field),
    });
  };

  ftxui::Elements artists{ftxui::text("Top artists") | ftxui::bold | ftxui::color(theme.title)};
  const auto top_artists = stats.GetTopArtists(kTopCount);

  for (std::size_t i = 0; i < top_artists.size(); i++) {
    artists.push_back(entry(i, top_artists[i].name, top_artists[i].listened_ms));
  }

  ftxui::Elements songs{ftxui::text("Top songs") | ftxui::bold | ftxui::color(theme.title)};
  const auto top_songs = stats.GetTopSongs(kTopCount);

  for (std::size_t i = 0; i < top_songs.size(); i++) {
    const auto& song = top_songs[i];
    songs.push_back(entry(i, song.artist.empty() ? song.title : song.title + " · " + song.artist,
                          song.listened_ms));
  }

  // Both lists get the same width, as names do not ask for any space (they are cut to fit)
  auto column = [](ftxui::Elements entries) {
    return ftxui::vbox(std::move(entries)) | ftxui::xflex;
  };

  return ftxui::hbox({
      column(std::move(artists)),
      ftxui::text(std::string(kColumnGap, ' ')),
      column(std::move(songs)),
  });
}

}  // namespace interface
