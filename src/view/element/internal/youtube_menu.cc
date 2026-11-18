#include "view/element/internal/youtube_menu.h"

#include "view/element/util.h"

namespace interface::internal {

YoutubeMenu::YoutubeMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                         const TextAnimation::Callback& force_refresh, const Callback& on_click,
                         const StateCallback& get_state)
    : BaseMenu(dispatcher, force_refresh), on_click_{on_click}, get_state_{get_state} {}

/* ********************************************************************************************** */

void YoutubeMenu::UpdateStyleImpl() {
  const auto& theme = GetTheme();

  style_ = Style{
      .mark = ftxui::color(theme.dialog.menu_tag) | ftxui::bold,
      .missing = ftxui::color(theme.dialog.error) | ftxui::bold,
      .detail = ftxui::color(theme.dialog.hint),
      .fallback = ftxui::color(theme.dialog.hint),
      .entry = Colored(theme.dialog.menu_song),
  };
}

/* ********************************************************************************************** */

ftxui::Element YoutubeMenu::RenderImpl() {
  using Kind = Row::Kind;

  auto max_size =
      GetMaxColumns() ? ftxui::size(ftxui::WIDTH, ftxui::EQUAL, GetMaxColumns()) : ftxui::nothing;

  ftxui::Elements menu_entries;
  menu_entries.reserve(entries_.size());

  const int selected = *GetSelected();
  const int focused = *GetFocused();
  auto& boxes = GetBoxes();

  for (int i = 0; i < GetSize(); ++i) {
    const auto& entry = entries_.at(i);
    const RowState state = get_state_ ? get_state_(entry) : RowState{};

    bool is_focused = (focused == i);
    bool is_selected = (selected == i);

    const auto& type = style_.entry;
    ftxui::Decorator style = is_selected ? (is_focused ? type.selected_focused : type.selected)
                                         : (is_focused ? type.focused : type.normal);

    const bool fallback = entry.kind == Kind::SearchChannels || entry.kind == Kind::SearchPlaylists;
    if (fallback && !is_selected && !is_focused) style = style_.fallback;

    // Release has an arrow telling whether it is expanded, and its tracks are indented below it
    std::string lead;
    if (entry.kind == Kind::Release) lead = state.expanded ? "▾ " : "▸ ";
    if (entry.kind == Kind::Track) lead = std::string(kArrowColumns + kIndentColumns, ' ');

    std::string mark;
    if (state.mark.has_value()) {
      switch (*state.mark) {
        case Mark::None:
          mark = "[ ] ";
          break;
        case Mark::Some:
          mark = "[~] ";
          break;
        case Mark::All:
          mark = "[x] ";
          break;
        case Mark::Missing:
          mark = "[!] ";
          break;
      }
    }

    const std::string detail = state.detail.value_or(entry.detail);

    menu_entries.push_back(
        ftxui::hbox({
            RenderPrefix(is_selected),
            ftxui::text(lead) | style_.detail,
            ftxui::text(mark) | (state.mark == Mark::Missing ? style_.missing : style_.mark),
            ellipsized_text(entry.text, 1) | style | ftxui::xflex,
            ftxui::text(detail.empty() ? "" : " " + detail) | style_.detail,
        }) |
        max_size | (is_focused ? ftxui::select : ftxui::nothing) | ftxui::reflect(boxes[i]));
  }

  // Box is the whole space for entries (even when empty), so mouse wheel works anywhere on it
  return ftxui::vbox(menu_entries) | ftxui::yframe | ftxui::flex | ftxui::reflect(Box());
}

/* ********************************************************************************************** */

bool YoutubeMenu::OnClickImpl() {
  auto active = GetActiveEntryImpl();
  if (!active.has_value()) return false;

  return on_click_(*active);
}

/* ********************************************************************************************** */

std::optional<YoutubeMenu::Row> YoutubeMenu::GetActiveEntryImpl() const {
  const int index = GetSelected();

  if (index < 0 || index >= GetSizeImpl()) return std::nullopt;
  return entries_.at(index);
}

}  // namespace interface::internal
