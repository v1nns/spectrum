#include "view/block/main_tab.h"

#include <memory>
#include <vector>

#include "util/logger.h"
#include "view/block/main_content/audio_equalizer.h"
#include "view/block/main_content/song_lyric.h"
#include "view/block/main_content/spectrum_visualizer.h"

namespace interface {

MainTab::MainTab(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, model::BlockIdentifier::MainTab, interface::Size{.width = 0, .height = 0}},
      tab_elem_{} {
  // Create all tabs
  tab_elem_[View::Visualizer] = std::make_unique<SpectrumVisualizer>(
      GetId(), dispatcher, std::bind(&MainTab::AskForFocus, this), "1");

  tab_elem_[View::Equalizer] = std::make_unique<AudioEqualizer>(
      GetId(), dispatcher, std::bind(&MainTab::AskForFocus, this), "2");

  tab_elem_[View::Lyric] =
      std::make_unique<SongLyric>(GetId(), dispatcher, std::bind(&MainTab::AskForFocus, this), "3");

  // Set visualizer as active tab
  tab_elem_.SetActive(View::Visualizer);

  // Create general tab buttons
  CreateButtons();
}

/* ********************************************************************************************** */

ftxui::Element MainTab::Render() {
  // Toggle flag only if it was enabled
  if (is_fullscreen_) is_fullscreen_ = false;

  ftxui::Elements buttons;

  // Append tab buttons
  for (const auto& [id, item] : tab_elem_.items()) {
    buttons.emplace_back(item->GetButton()->Render());
  }

  // Append general buttons
  buttons.insert(buttons.end(),
                 {
                     ftxui::filler(),
                     btn_help_->Render(),
                     ftxui::text(" ") | ftxui::border,  // dummy space between buttons
                     btn_exit_->Render(),
                 });

  ftxui::Element title_border = ftxui::hbox(buttons);

  ftxui::Element view = tab_elem_.active_item()->Render();

  return ftxui::window(title_border, view | ftxui::yflex) | GetBorderDecorator();
}

/* ********************************************************************************************** */

ftxui::Element MainTab::RenderFullscreen() {
  // Toggle flag only if it was disabled
  if (!is_fullscreen_) is_fullscreen_ = true;

  return tab_elem_.active_item()->Render();
}

/* ********************************************************************************************** */

bool MainTab::OnEvent(ftxui::Event event) {
  if (event.is_mouse()) return OnMouseEvent(event);

  // Check if event is equal to a registered keybinding for any of the tab items
  if (auto found = std::find_if(
          tab_elem_.items().begin(), tab_elem_.items().end(),
          [&event](const auto& t) { return t.second->GetKeybinding() == event.character(); });
      found != tab_elem_.items().end() && !is_fullscreen_) {
    // Ask for focus if block is not focused
    if (!IsFocused()) {
      LOG("Handle key to focus tab viewer block");
      AskForFocus();
    }

    // Change tab item selected
    if (tab_elem_.active() != found->first) {
      LOG("Handle key to change tab item selected");
      tab_elem_.SetActive(found->first);
    }

    return true;
  }

  // If block is not focused, do not even try to handle event
  if (!IsFocused()) {
    return false;
  }

  // Otherwise, let item handle it
  return tab_elem_.active_item()->OnEvent(event);
}

/* ********************************************************************************************** */

bool MainTab::OnCustomEvent(const CustomEvent& event) {
  // Even if TabItem::SongLyrics is not active, force it to process these events
  // By doing this, it makes possible to fetch lyrics on background
  if ((event == CustomEvent::Identifier::ClearSongInfo ||
       event == CustomEvent::Identifier::UpdateSongInfo) &&
      tab_elem_.active() != View::Lyric) {
    tab_elem_[View::Lyric]->OnCustomEvent(event);
  }

  return tab_elem_.active_item()->OnCustomEvent(event);
}

/* ********************************************************************************************** */

void MainTab::OnFocus() {
  // Update internal state for all buttons
  for (const auto& [id, item] : tab_elem_.items()) item->GetButton()->UpdateParentFocus(true);

  btn_help_->UpdateParentFocus(true);
  btn_exit_->UpdateParentFocus(true);
}

/* ********************************************************************************************** */

void MainTab::OnLostFocus() {
  // Update internal state for all buttons
  for (const auto& [id, item] : tab_elem_.items()) item->GetButton()->UpdateParentFocus(false);

  btn_help_->UpdateParentFocus(false);
  btn_exit_->UpdateParentFocus(false);
}

/* ********************************************************************************************** */

int MainTab::GetBarWidth() {
  auto visualizer = static_cast<SpectrumVisualizer*>(tab_elem_[View::Visualizer].get());
  return visualizer->GetBarWidth();
}

/* ********************************************************************************************** */

bool MainTab::OnMouseEvent(ftxui::Event event) {
  if (btn_help_->OnMouseEvent(event)) return true;

  if (btn_exit_->OnMouseEvent(event)) return true;

  for (const auto& [id, item] : tab_elem_.items()) {
    if (item->GetButton()->OnMouseEvent(event)) {
      tab_elem_.SetActive(id);
      return true;
    }
  }

  return tab_elem_.active_item()->OnMouseEvent(event);
}

/* ********************************************************************************************** */

void MainTab::CreateButtons() {
  const auto button_style = Button::ButtonStyle{
      .focused =
          Button::ButtonStyle::State{
              .foreground = ftxui::Color::GrayLight,
              .background = ftxui::Color::GrayDark,
          },
      .delimiters = Button::Delimiters{"[", "]"},
  };

  btn_help_ = Button::make_button_for_window(
      std::string("F1:help"),
      [this]() {
        auto disp = GetDispatcher();

        LOG("Handle left click mouse event on Help button");
        auto event = interface::CustomEvent::ShowHelper();
        disp->SendEvent(event);

        return true;
      },
      button_style);

  btn_exit_ = Button::make_button_for_window(
      std::string("X"),
      [this]() {
        auto disp = GetDispatcher();

        LOG("Handle left click mouse event on Exit button");
        auto event = interface::CustomEvent::Exit();
        disp->SendEvent(event);

        return true;
      },
      button_style);
}

}  // namespace interface
