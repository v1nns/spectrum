/**
 * \file
 * \brief  Class for tab view containing audio equalizer control
 */

#ifndef INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_EQUALIZER_H_
#define INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_EQUALIZER_H_

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "ftxui/component/component.hpp"
#include "model/audio_filter.h"
#include "util/formatter.h"
#include "view/base/element.h"
#include "view/base/keybinding.h"
#include "view/element/button.h"
#include "view/element/focus_controller.h"
#include "view/element/style.h"
#include "view/element/tab.h"
#include "view/element/util.h"

#ifdef ENABLE_TESTS
namespace {
class MainContentTest;
}
#endif

namespace interface {

/**
 * @brief Component to control multiple frequency bars, in order to setup audio equalization
 */
class AudioEqualizer : public TabItem {
  static constexpr std::string_view kTabName = "equalizer";        //!< Tab title
  static constexpr std::string_view kModifiablePreset = "Custom";  //!< Only preset modifiable

 public:
  /**
   * @brief Construct a new AudioEqualizer object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   * @param on_focus Callback function to ask for focus
   * @param keybinding Keybinding to set item as active
   */
  explicit AudioEqualizer(const model::BlockIdentifier& id,
                          const std::shared_ptr<EventDispatcher>& dispatcher,
                          const FocusCallback& on_focus, const keybinding::Key& keybinding);

  /**
   * @brief Destroy the AudioEqualizer object
   */
  ~AudioEqualizer() override = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event) override;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEvent(ftxui::Event& event) override;

  /**
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
  //! Private methods
 private:
  //! Handle mapped keyboard events for navigation
  bool OnNavigationEvent(const ftxui::Event& event);

  //! Create general buttons
  void CreateButtons();

  //! Update UI components state based on internal cache
  void UpdateButtonState();

  //! Link current EQ settings to UI components
  void LinkPresetToInterface(model::EqualizerPreset& preset);

  //! Update current preset selected
  void UpdatePreset(const model::MusicGenre& preset);

  //! Utility to return current EQ settings
  model::EqualizerPreset& current_preset() {
    auto preset = presets_.find(preset_name_);
    assert(preset != presets_.end());
    return preset->second;
  }

  /* ******************************************************************************************** */
  //! Internal structures

  struct FrequencyBar final : public Element {
    static constexpr int kMaxGainLength = 8;      //!< Maximum length in the input box for gain
    static constexpr int kCompactGainLength = 4;  //!< Same as above, but when space is limited
    static constexpr int kKiloHertz = 1000;       //!< Used to format frequency in compact mode

    //! Style for frequency bar
    struct BarStyle {
      ftxui::Color background;
      ftxui::Color foreground;
    };

    //!< Color styles
    BarStyle style_normal =
        BarStyle{.background = GetTheme().equalizer.bar.background,
                 .foreground = GetTheme().equalizer.bar.foreground};  //!< Normal mode

    BarStyle style_hovered =
        BarStyle{.background = GetTheme().equalizer.bar_hovered.background,
                 .foreground = GetTheme().equalizer.bar_hovered.foreground};  //!< On hover state

    BarStyle style_focused =
        BarStyle{.background = GetTheme().equalizer.bar_focused.background,
                 .foreground = GetTheme().equalizer.bar_focused.foreground};  //!< On focus state

    model::AudioFilter* filter;  //!< Audio frequency filters for equalization

    /**
     * @brief Render frequency bar
     * @return UI element
     */
    ftxui::Element Render() override { return Draw(false); }

    /**
     * @brief Render frequency bar
     * @param compact Use shorter labels (without units), for when there is not much space available
     * @return UI element
     */
    ftxui::Element Draw(bool compact) {
      using ftxui::EQUAL;
      using ftxui::WIDTH;

      constexpr auto empty_line = []() { return ftxui::text(""); };

      constexpr auto gen_slider = [&](float value, const BarStyle& style) {
        ftxui::Decorator color = ftxui::bgcolor(style.background) | ftxui::color(style.foreground);

        return ftxui::hbox({
                   ftxui::gaugeUp(value) | ftxui::yflex_grow | color,
                   ftxui::gaugeUp(value) | ftxui::yflex_grow | color,
               }) |
               ftxui::hcenter | ftxui::yflex_grow;
      };

      // Get gain value and choose style
      float gain = filter->GetGainAsPercentage();
      const BarStyle* style;

      style = IsFocused() ? &style_focused : IsHovered() ? &style_hovered : &style_normal;

      return ftxui::vbox({
          // title
          empty_line(),
          ftxui::text(compact ? GetCompactFrequency() : filter->GetFrequency()) |
              ftxui::color(GetTheme().equalizer.text) | ftxui::hcenter,
          empty_line(),

          // frequency gauge
          gen_slider(gain, *style) | ftxui::reflect(Box()),

          // gain input
          empty_line(),
          ftxui::text(compact ? GetCompactGain() : filter->GetGain()) |
              ftxui::color(GetTheme().equalizer.text) | ftxui::inverted | ftxui::hcenter |
              ftxui::size(WIDTH, EQUAL, compact ? kCompactGainLength : kMaxGainLength),
          empty_line(),
      });
    }

    //! Format gain without unit, centered in the input box (to highlight the whole box)
    [[nodiscard]] std::string GetCompactGain() const {
      std::string gain = util::to_string_with_precision(filter->gain, 0);

      const int padding = kCompactGainLength - static_cast<int>(gain.size());
      if (padding <= 0) {
        return gain;
      }

      const int left = padding / 2;
      return std::string(left, ' ') + gain + std::string(padding - left, ' ');
    }

    //! Format frequency without unit (e.g. "125" for 125 Hz and "16k" for 16 kHz)
    [[nodiscard]] std::string GetCompactFrequency() const {
      const auto frequency = static_cast<int>(filter->frequency);

      return frequency < kKiloHertz ? std::to_string(frequency)
                                    : std::to_string(frequency / kKiloHertz) + "k";
    }

   private:
    /**
     * @brief Handles an action key event (arrow keys or hjkl)
     * @param event Received event from screen
     */
    bool HandleActionKey(const ftxui::Event& event) override {
      if (!filter->modifiable) return false;

      // Increment value and update UI
      if (event == keybinding::Navigation::ArrowUp || event == keybinding::Navigation::Up) {
        double gain = filter->gain + 1;
        filter->SetNormalizedGain(gain);
        return true;
      }

      // Decrement value and update UI
      if (event == keybinding::Navigation::ArrowDown || event == keybinding::Navigation::Down) {
        double gain = filter->gain - 1;
        filter->SetNormalizedGain(gain);
        return true;
      }

      return false;
    }

    /**
     * @brief Handles a mouse scroll wheel event
     * @param button Received button event from screen
     */
    void HandleWheel(const ftxui::Mouse::Button& button) override {
      if (!filter->modifiable) return;

      double increment = button == ftxui::Mouse::WheelUp ? 1 : -1;
      filter->SetNormalizedGain(filter->gain + increment);
    }

    /**
     * @brief Handles a mouse click event
     * @param event Received event from screen
     */
    void HandleClick(ftxui::Event& event) override {
      if (!filter->modifiable) return;

      const auto box = Box();

      // Calculate new value for gain based on coordinates from mouse click and bar size
      double value = std::ceil(model::AudioFilter::kMaxGain -
                               (event.mouse().y - box.y_min) *
                                   (model::AudioFilter::kMaxGain - model::AudioFilter::kMinGain) /
                                   (box.y_max - box.y_min));

      filter->SetNormalizedGain(value);
    }
  };

  /* ******************************************************************************************** */

  struct GenrePicker final : public Element {
    static constexpr int kMaxHeight = 8;  //!< Maximum height for this element
    static constexpr int kMaxWidth = 13;  //!< Maximum width for this element

    std::vector<model::MusicGenre> presets;  //!< All available presets
    int entry_focused = 0;                   //!< Index for entry selected (title + presets list)
    int entry_hovered = -1;                  //!< Index for entry focused (default is none)
    bool title_hovered = false;              //!< Flag to control hover state on title

    model::MusicGenre* preset_name;  //!< Current preset

    //! Callback to inform external TabView (this AudioVisualizer) to update its current preset
    using Callback = std::function<void(const model::MusicGenre&)>;
    Callback update_preset;  //!< Notify AudioVisualizer to update preset

    std::vector<ftxui::Box> boxes;  //!< Single box for each entry
    bool opened = false;            //!< Control if element is opened, to list all presets

    /**
     * @brief Initialize this element with data from TabView
     * @param eq_presets Equalizer presets
     * @param name Current preset
     */
    void Initialize(const model::EqualizerPresets& eq_presets, model::MusicGenre* name,
                    const Callback& update) {
      presets.reserve(eq_presets.size());
      boxes.resize(eq_presets.size() + 1);  // presets + title

      for (const auto& [genre, filters] : eq_presets) presets.push_back(genre);

      preset_name = name;
      update_preset = update;
    }

    /**
     * @brief Render music genre EQ picker
     * @return UI element
     */
    ftxui::Element Render() override {
      using ftxui::EQUAL;
      using ftxui::HEIGHT;
      using ftxui::WIDTH;

      ftxui::Elements entries;

      auto prefix = ftxui::text(opened ? "↓ " : "→ ");
      auto title = ftxui::text(*preset_name);

      if ((IsFocused() && entry_focused == 0) || title_hovered) {
        title |= ftxui::inverted;
      }

      entries.reserve(opened ? presets.size() + 2 : 1);  // presets + title + separator
      entries.push_back(ftxui::hbox({prefix, title}) | ftxui::reflect(boxes[0]));

      if (opened) {
        entries.push_back(ftxui::separator());

        // Note: +1 or -1 below are used to ignore the title index
        for (int i = 0; i < presets.size(); i++) {
          bool active = presets[i] == *preset_name;
          bool is_focused = (IsFocused() && i == (entry_focused - 1)) ||
                            (IsHovered() && i == (entry_hovered - 1));

          auto state = ftxui::EntryState{
              presets[i],
              active,
              active,
              is_focused,
          };

          entries.push_back(ftxui::RadioboxOption::Simple().transform(state) |
                            ftxui::reflect(boxes[i + 1]));
        }
      }

      auto content = ftxui::vbox(entries) | ftxui::size(WIDTH, EQUAL, kMaxWidth);
      if (opened) content |= ftxui::size(HEIGHT, EQUAL, kMaxHeight);

      return ftxui::vbox({
                 ftxui::filler(),
                 content | ftxui::center | ftxui::border | ftxui::reflect(Box()),
                 ftxui::filler(),
             }) |
             ftxui::color(GetTheme().equalizer.text);
    }

   private:
    /**
     * @brief Handles an action key event (arrow keys or hjkl)
     * @param event Received event from screen
     */
    bool HandleActionKey(const ftxui::Event& event) override {
      if (event == keybinding::Navigation::Space || event == keybinding::Navigation::Return) {
        // Open element
        if (!opened) {
          opened = true;
          return false;
        }

        // Close element
        if (entry_focused == 0) {
          opened = false;
          return false;
        }

        // Select a new preset
        int offset = entry_focused - 1;
        if (presets[offset] != *preset_name) {
          update_preset(presets[offset]);
        }
      }

      // While closed, cycle through presets
      if (!opened) {
        bool previous =
            event == keybinding::Navigation::ArrowUp || event == keybinding::Navigation::Up;
        bool next =
            event == keybinding::Navigation::ArrowDown || event == keybinding::Navigation::Down;

        if (!previous && !next) return false;

        auto it = std::find(presets.begin(), presets.end(), *preset_name);
        int size = static_cast<int>(presets.size());
        int index = it != presets.end() ? static_cast<int>(it - presets.begin()) : 0;

        index = (index + (next ? 1 : size - 1)) % size;
        update_preset(presets[index]);

        return true;
      }

      if (event == keybinding::Navigation::ArrowDown || event == keybinding::Navigation::Down) {
        entry_focused = entry_focused + (entry_focused < static_cast<int>(presets.size()) ? 1 : 0);
      }

      if (event == keybinding::Navigation::ArrowUp || event == keybinding::Navigation::Up) {
        entry_focused = entry_focused - (entry_focused > 0 ? 1 : 0);
      }

      return true;
    }

    /**
     * @brief Handles a mouse scroll wheel event
     * @param button Received button event from screen
     */
    void HandleWheel(const ftxui::Mouse::Button& button) override {
      // Update index based on internal state (if focused or hovered)
      auto update_index = [this, &button](int& index) {
        if (button == ftxui::Mouse::WheelUp)
          index = index - (index > 0 ? 1 : 0);
        else if (button == ftxui::Mouse::WheelDown) {
          index = index + (index < static_cast<int>(presets.size()) ? 1 : 0);
        }
      };

      if (opened) {
        update_index(IsFocused() ? entry_focused : entry_hovered);
      }
    }

    /**
     * @brief Handles a mouse click event
     * @param event Received event from screen
     */
    void HandleClick(ftxui::Event& event) override {
      for (int i = 0; i < boxes.size(); i++) {
        if (boxes[i].Contain(event.mouse().x, event.mouse().y)) {
          if (i == 0) {
            // Click on title, so change opened state
            opened = !opened;
          } else {
            // Otherwise, it is a click on preset, so fix offset and use it to update current preset
            --i;
            update_preset(presets[i]);
          }
          break;
        }
      }
    }

    /**
     * @brief Handles a mouse hover event
     * @param event Received event from screen
     */
    void HandleHover(ftxui::Event& event) override {
      bool found = false;
      for (int i = 0; i < boxes.size(); i++) {
        if (boxes[i].Contain(event.mouse().x, event.mouse().y)) {
          title_hovered = i == 0 ? true : false;
          entry_hovered = i;
          found = true;
          break;
        }
      }

      // Clear indexes
      if (!found) {
        entry_hovered = -1;
        title_hovered = false;
      }
    }
  };

  /* ******************************************************************************************** */

  //! Cache for last applied preset
  struct PresetApplied {
    model::MusicGenre genre;
    model::EqualizerPreset preset;

    //! Overloaded operators
    bool operator==(const model::EqualizerPreset& other) const { return preset == other; }
    bool operator!=(const model::EqualizerPreset& other) const { return !operator==(other); }

    /**
     * @brief Update internal cache
     * @param genre_updated New genre
     * @param preset_updated New preset
     */
    void Update(const model::MusicGenre& updated_genre,
                const model::EqualizerPreset& updated_preset) {
      genre = updated_genre;
      preset = updated_preset;
    }
  };

  /* ******************************************************************************************** */
  //! Variables

  //! Equalizer settings
  PresetApplied last_applied_;  //!< Last EQ settings applied

  model::EqualizerPresets presets_ =
      model::AudioFilter::CreatePresets();  //!< List of EQ settings available to use

  /* ******************************************************************************************** */
  //! Interface elements

  GenrePicker picker_;  //!< EQ picker

  using FrequencyBars = std::array<FrequencyBar, model::equalizer::kFiltersPerPreset>;
  FrequencyBars bars_;  //!< Array of gauges for EQ settings

  GenericButton btn_apply_;  //!< Buttons to apply equalization
  GenericButton btn_reset_;  //!< Buttons to reset equalization

  /* ******************************************************************************************** */
  //! Internal focus handling

  FocusController focus_ctl_;  //!< Controller to manage focus in registered elements
  model::MusicGenre preset_name_ =
      model::MusicGenre(kModifiablePreset);  //!< Index name to current EQ settings

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::MainContentTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_EQUALIZER_H_
