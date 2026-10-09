/**
 * \file
 * \brief  Class for tab view containing audio equalizer control
 */

#ifndef INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_EQUALIZER_H_
#define INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_EQUALIZER_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
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
 * @brief Node that renders a vertical slider for gain: a thin track with a knob placed according
 * to gain, which is in the middle of track when there is no gain (marked by a tick), at its top
 * with the maximum gain and at its bottom with the minimum one
 */
class GainSlider : public ftxui::Node {
  static constexpr int kMinLines = 3;       //!< Line for no gain and one line for each direction
  static constexpr int kLevelsPerLine = 8;  //!< Knob is moved in eighths of a line

 public:
  static constexpr int kColumns = 3;  //!< Track uses the column in the middle, knob uses them all

  /**
   * @brief Construct a new slider
   * @param gain Value for gain
   * @param colors Foreground for knob (and track between it and no gain), background for track
   */
  GainSlider(double gain, const Theme::State& colors) : gain_{gain}, colors_{colors} {}

  void ComputeRequirement() override {
    requirement_.min_x = kColumns;
    requirement_.min_y = kMinLines;
  }

  void Render(ftxui::Screen& screen) override {
    // Block elements, each one filling a line from its bottom (index is the level)
    static constexpr std::array<std::string_view, kLevelsPerLine + 1> kBlocks{
        " ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█",
    };

    const int height = box_.y_max - box_.y_min + 1;
    if (height < kMinLines) return;

    // Line for no gain, and where knob starts (in levels, counted from the top of slider)
    const int zero = (height - 1) / 2;

    auto levels = [this](double limit, int lines) {
      const double ratio = std::clamp(gain_ / limit, 0.0, 1.0);
      return static_cast<int>(std::round(ratio * lines * kLevelsPerLine));
    };

    const int knob = (zero * kLevelsPerLine) - levels(model::AudioFilter::kMaxGain, zero) +
                     levels(model::AudioFilter::kMinGain, height - 1 - zero);

    const int track = box_.x_min + ((box_.x_max - box_.x_min) / 2);

    for (int line = 0; line < height; line++) {
      // Part of this line used by knob (which may be placed between two lines)
      const int top = line * kLevelsPerLine;
      const int begin = std::clamp(knob - top, 0, kLevelsPerLine);
      const int end = std::clamp(knob + kLevelsPerLine - top, 0, kLevelsPerLine);

      // Track is highlighted from the line for no gain until the knob
      const bool filled = (line > zero && top < knob) || (line < zero && top > knob);

      for (int x = box_.x_min; x <= box_.x_max; x++) {
        auto& pixel = screen.PixelAt(x, box_.y_min + line);

        if (end > begin) {
          pixel.foreground_color = colors_.foreground;

          if (end == kLevelsPerLine) {
            // Knob uses the bottom of this line, which is exactly what a block element does
            pixel.character = kBlocks.at(kLevelsPerLine - begin);
          } else {
            // Knob uses the top of this line and there is no block element for that, so draw the
            // part that must stay empty and swap its colors
            pixel.character = kBlocks.at(kLevelsPerLine - end);
            pixel.inverted = true;
          }
        } else if (line == zero) {
          pixel.character = x == track ? "┼" : "─";
          pixel.foreground_color = colors_.background;
        } else if (x == track) {
          pixel.character = filled ? "┃" : "│";
          pixel.foreground_color = filled ? colors_.foreground : colors_.background;
        }
      }
    }
  }

 private:
  double gain_;          //!< Value for gain
  Theme::State colors_;  //!< Colors for slider
};

/**
 * @brief Component to control multiple frequency bars, in order to setup audio equalization
 */
class AudioEqualizer : public TabItem {
  static constexpr std::string_view kTabName = "equalizer";  //!< Tab title
  //! Only preset modifiable
  static constexpr std::string_view kModifiablePreset = model::equalizer::kCustomPreset;
  static constexpr int kMarginColumns = 2;  //!< Empty columns on both sides of content

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

  //! Render scale for gain, shown before frequency bars (with units for frequency and gain)
  ftxui::Element RenderScale() const;

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
    static constexpr int kColumns = 5;       //!< Columns for the whole element (labels included)
    static constexpr int kKiloHertz = 1000;  //!< Used to format frequency

    //! Style for frequency bar
    using BarStyle = Theme::State;

    model::AudioFilter* filter;  //!< Audio frequency filters for equalization

    /**
     * @brief Render frequency bar as a slider for its gain, with its frequency above it and its
     * gain below it
     * @return UI element
     */
    ftxui::Element Render() override {
      using ftxui::EQUAL;
      using ftxui::WIDTH;

      // Choose style
      const auto& theme = GetTheme().equalizer;
      const bool highlighted = IsFocused() || IsHovered();
      const BarStyle& style = IsFocused()   ? theme.bar_focused
                              : IsHovered() ? theme.bar_hovered
                                            : theme.bar;

      ftxui::Decorator label =
          highlighted ? ftxui::color(style.foreground) | ftxui::bold : ftxui::color(theme.text);

      ftxui::Element bar = std::make_shared<GainSlider>(filter->gain, style) |
                           ftxui::size(WIDTH, EQUAL, GainSlider::kColumns) | ftxui::hcenter |
                           ftxui::yflex_grow | ftxui::reflect(Box());

      return ftxui::vbox({
                 ftxui::text(GetFrequency()) | label | ftxui::hcenter,
                 ftxui::text(""),
                 bar,
                 ftxui::text(""),
                 ftxui::text(GetGain()) | label | ftxui::hcenter,
             }) |
             ftxui::size(WIDTH, EQUAL, kColumns);
    }

    //! Format gain without unit, with a sign when it is increased (e.g. "+3", "0" and "-2")
    [[nodiscard]] std::string GetGain() const {
      const std::string gain = util::to_string_with_precision(filter->gain, 0);
      return filter->gain > 0 ? "+" + gain : gain;
    }

    //! Format frequency without unit (e.g. "125" for 125 Hz and "16k" for 16 kHz)
    [[nodiscard]] std::string GetFrequency() const {
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
    static constexpr int kMaxWidth = 12;      //!< Maximum width for a preset name
    static constexpr int kPrefixColumns = 2;  //!< Used by what is shown before a preset name
    static constexpr int kScrollColumns = 1;  //!< Used by scroll indicator from list of presets

    static constexpr std::string_view kLabel = "preset ";    //!< Shown before current preset
    static constexpr std::string_view kPrefixClosed = "→ ";  //!< Shown while list is closed
    static constexpr std::string_view kPrefixOpened = "↓ ";  //!< Shown while list is opened

    //! Columns for title (label, prefix and name from current preset)
    static constexpr int kTitleWidth = static_cast<int>(kLabel.size()) + kPrefixColumns + kMaxWidth;

    std::vector<model::MusicGenre> presets;  //!< All available presets
    int entry_focused = 0;                   //!< Index for entry selected (title + presets list)
    int entry_hovered = -1;                  //!< Index for entry focused (default is none)
    bool title_hovered = false;              //!< Flag to control hover state on title

    model::MusicGenre* preset_name;  //!< Current preset

    //! Callback to inform external TabView (this AudioVisualizer) to update its current preset
    using Callback = std::function<void(const model::MusicGenre&)>;
    Callback update_preset;  //!< Notify AudioVisualizer to update preset

    std::vector<ftxui::Box> boxes;  //!< Single box for each entry
    ftxui::Box list_box;            //!< Box for list of presets (with its border)
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
     * @brief Render current preset in a single line (while list is opened, only the space for it,
     * as the whole element is rendered above everything else by RenderOpened)
     * @return UI element
     */
    ftxui::Element Render() override {
      using ftxui::EQUAL;
      using ftxui::WIDTH;

      if (opened) return ftxui::text("") | ftxui::size(WIDTH, EQUAL, kTitleWidth);

      return RenderTitle() | ftxui::reflect(Box());
    }

    /**
     * @brief Render current preset with the list of presets right below it
     * @return UI element
     */
    ftxui::Element RenderOpened() {
      using ftxui::EQUAL;
      using ftxui::WIDTH;

      ftxui::Elements entries;
      entries.reserve(presets.size());

      // Note: +1 or -1 below are used to ignore the title index
      for (int i = 0; i < presets.size(); i++) {
        bool active = presets[i] == *preset_name;
        bool is_focused =
            (IsFocused() && i == (entry_focused - 1)) || (IsHovered() && i == (entry_hovered - 1));

        auto state = ftxui::EntryState{
            presets[i],
            active,
            active,
            is_focused,
        };

        // Entry focused (or the one from current preset) is always kept visible, as not all
        // entries may fit in the space available for list
        const bool visible = entry_focused > 0 ? i == (entry_focused - 1) : active;

        entries.push_back(ftxui::RadioboxOption::Simple().transform(state) |
                          (visible ? ftxui::focus : ftxui::nothing) | ftxui::reflect(boxes[i + 1]));
      }

      // As list is rendered above other elements, it must fill its own background (with the one
      // from screen, which may not be the one from terminal)
      ftxui::Element list = ftxui::vbox(entries) | ftxui::vscroll_indicator | ftxui::yframe |
                            ftxui::size(WIDTH, EQUAL, kPrefixColumns + kMaxWidth + kScrollColumns) |
                            ftxui::color(GetTheme().equalizer.text) | ftxui::border |
                            ftxui::bgcolor(GetTheme().screen.background) |
                            ftxui::reflect(list_box);

      // List uses only the columns it needs, instead of all the ones used by title
      return ftxui::vbox({
                 RenderTitle(),
                 ftxui::hbox({list | ftxui::clear_under, ftxui::filler()}),
             }) |
             ftxui::reflect(Box());
    }

    /**
     * @brief Handles an event (from keyboard)
     * @param event Received event from screen
     * @return true if event was handled, otherwise false
     */
    bool OnEvent(const ftxui::Event& event) override {
      // Close list of presets, keeping focus on this element
      if (opened && event == keybinding::Navigation::Escape) {
        Close();
        return true;
      }

      return false;
    }

    /**
     * @brief Close list of presets when mouse is clicked on anything else
     * @param event Received event from screen
     * @return true if list was closed, otherwise false
     */
    bool CloseOnClickOutside(ftxui::Event& event) {
      const auto& mouse = event.mouse();

      if (!opened || mouse.button != ftxui::Mouse::Left ||
          mouse.motion != ftxui::Mouse::Released) {
        return false;
      }

      // Title is the first box, and it is not part of list
      if (boxes[0].Contain(mouse.x, mouse.y) || list_box.Contain(mouse.x, mouse.y)) return false;

      Close();
      return true;
    }

   private:
    //! Open list of presets, with focus on the current one
    void Open() {
      auto it = std::find(presets.begin(), presets.end(), *preset_name);

      // Note: +1 is used to ignore the title index
      entry_focused = it != presets.end() ? static_cast<int>(it - presets.begin()) + 1 : 0;
      opened = true;
    }

    //! Close list of presets, with focus back on title
    void Close() {
      entry_focused = 0;
      opened = false;
    }

    //! Render label and current preset
    ftxui::Element RenderTitle() {
      using ftxui::EQUAL;
      using ftxui::WIDTH;

      const auto& theme = GetTheme().equalizer;

      auto prefix = ftxui::text(std::string{opened ? kPrefixOpened : kPrefixClosed});
      auto title = ftxui::text(*preset_name);

      if ((IsFocused() && entry_focused == 0) || title_hovered) {
        title |= ftxui::inverted;
      }

      return ftxui::hbox({
                 ftxui::text(std::string{kLabel}) | ftxui::color(theme.label),
                 ftxui::hbox({prefix, title}) | ftxui::color(theme.text),
             }) |
             ftxui::size(WIDTH, EQUAL, kTitleWidth) | ftxui::reflect(boxes[0]);
    }

    /**
     * @brief Handles an action key event (arrow keys or hjkl)
     * @param event Received event from screen
     */
    bool HandleActionKey(const ftxui::Event& event) override {
      if (event == keybinding::Navigation::Space || event == keybinding::Navigation::Return) {
        // Open element
        if (!opened) {
          Open();
          return false;
        }

        // Close element
        if (entry_focused == 0) {
          Close();
          return false;
        }

        // Select a new preset (list is not needed anymore, as preset was chosen)
        int offset = entry_focused - 1;
        if (presets[offset] != *preset_name) {
          update_preset(presets[offset]);
        }

        Close();
        return true;
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
        return;
      }

      // While closed, cycle through presets exactly like its keys
      HandleActionKey(button == ftxui::Mouse::WheelUp ? keybinding::Navigation::ArrowUp
                                                      : keybinding::Navigation::ArrowDown);
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
            if (opened) {
              Close();
            } else {
              Open();
            }
          } else {
            // Otherwise, it is a click on preset, so fix offset and use it to update current preset
            // (list is not needed anymore, as preset was chosen)
            --i;
            update_preset(presets[i]);
            Close();
          }
          break;
        }
      }
    }

    /**
     * @brief Handles a mouse double click event, which is just another click here (otherwise, a
     * click on preset right after opening list would be ignored)
     * @param event Received event from screen
     */
    void HandleDoubleClick(ftxui::Event& event) override { HandleClick(event); }

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
