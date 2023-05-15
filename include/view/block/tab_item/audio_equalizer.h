/**
 * \file
 * \brief  Class for tab view containing audio equalizer control
 */

#ifndef INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_
#define INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>
#include <type_traits>
#include <vector>

#include "ftxui/component/component.hpp"
#include "model/audio_filter.h"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/element/button.h"
#include "view/element/tab_item.h"

namespace interface {

/**
 * @brief Component to control multiple frequency bars, in order to setup audio equalization
 */
class AudioEqualizer : public TabItem {
  static constexpr std::string_view kModifiablePreset = "Custom";  //!< Only preset modifiable

 public:
  /**
   * @brief Construct a new AudioEqualizer object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   */
  explicit AudioEqualizer(const model::BlockIdentifier& id,
                          const std::shared_ptr<EventDispatcher>& dispatcher);

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
  bool OnMouseEvent(const ftxui::Event& event) override;

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

  //! Base class for elements inside this TabView
  struct Element {
    //! Default destructor
    virtual ~Element() = default;

    ftxui::Box box;        //!< Box to control if mouse cursor is over the element
    bool hovered = false;  //!< Flag to indicate if element is hovered (by mouse)
    bool focused = false;  //!< Flag to indicate if element is focused (set by equalizer)

    /**
     * @brief Handles an event (from mouse)
     * @param event Received event from screen
     * @return true if event was handled, otherwise false
     */
    bool OnMouseEvent(ftxui::Event event) {
      if (hovered && (event.mouse().button == ftxui::Mouse::WheelDown ||
                      event.mouse().button == ftxui::Mouse::WheelUp)) {
        HandleWheel(event.mouse().button);
        return true;
      }

      if (box.Contain(event.mouse().x, event.mouse().y)) {
        hovered = true;

        if (event.mouse().button == ftxui::Mouse::Left &&
            event.mouse().motion == ftxui::Mouse::Released) {
          HandleClick(event);
          return true;
        }

        HandleHover(event);
        return true;
      } else {
        hovered = false;
      }

      return false;
    }

    //! Implemented by derived class
    virtual ftxui::Element Render() { return ftxui::text(""); }
    virtual void HandleNavigationKey(const ftxui::Event& event) {
      // Optional implementation
    }
    virtual void HandleWheel(ftxui::Mouse::Button button) {
      // Optional implementation
    }
    virtual void HandleClick(ftxui::Event& event) {
      // Optional implementation
    }
    virtual void HandleHover(ftxui::Event& event) {
      // Optional implementation
    }
  };

  /* ******************************************************************************************** */

  //! Wrapper to control focus on elements based on external events
  struct FocusController final {
    static constexpr int kInvalidIndex = -1;  //!< Invalid index (when no element is focused)

    std::vector<Element*> elements;   //!< List of elements ordered by focus priority
    int focus_index = kInvalidIndex;  //!< Index to current element focused

    //!< List of mapped events to be handled as navigation key
    const std::array<ftxui::Event, 6> navigation_events{
        ftxui::Event::ArrowUp,        ftxui::Event::Character('k'), ftxui::Event::ArrowDown,
        ftxui::Event::Character('j'), ftxui::Event::Character(' '), ftxui::Event::Return,
    };

    /**
     * @brief Append elements to manage focus (using C++17 variadic template)
     *        P.S.: Focus priority is based on elements order in internal vector
     * @tparam ...Args Elements
     * @param ...args Elements to be appended
     */
    template <typename... Args>
    void Append(Args&... args) {
      elements.insert(elements.end(), {static_cast<decltype(elements)::value_type>(&args)...});
    }

    /**
     * @brief Append array of elements to manage focus (using SFINAE to enable only for iterators)
     *        P.S.: Focus priority is based on elements order in internal vector
     * @tparam Iterator Element container iterator
     * @param begin Initial element
     * @param end Last element
     */
    template <class Iterator,
              typename = std::is_same<typename std::iterator_traits<Iterator>::iterator_category,
                                      std::input_iterator_tag>>
    void Append(Iterator begin, Iterator end) {
      auto size = std::distance(begin, end);
      elements.reserve(elements.size() + size);

      for (auto it = begin; it != end; it++) elements.push_back(&*it);
    }

    /**
     * @brief Handles an event (from keyboard)
     * @param event Received event from screen
     * @return true if event was handled, otherwise false
     */
    bool OnEvent(const ftxui::Event& event) {
      // Navigate on elements
      if (event == ftxui::Event::ArrowRight || event == ftxui::Event::Character('l')) {
        LOG("Handle menu navigation key=", util::EventToString(event));

        // Calculate new index based on upper bound
        int new_index =
            focus_index + (focus_index < (static_cast<int>(elements.size()) - 1) ? 1 : 0);
        UpdateFocus(focus_index, new_index);

        return true;
      }

      // Navigate on elements
      if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::Character('h')) {
        LOG("Handle menu navigation key=", util::EventToString(event));

        // Calculate new index based on lower bound
        int new_index = focus_index - (focus_index > (kInvalidIndex + 1) ? 1 : 0);
        UpdateFocus(focus_index, new_index);

        return true;
      }

      if (IsElementFocused()) {
        // Pass event to element if mapped as navigation key
        if (auto found = std::find(navigation_events.begin(), navigation_events.end(), event);
            found != navigation_events.end()) {
          LOG("Handle menu navigation key=", util::EventToString(event));
          elements[focus_index]->HandleNavigationKey(event);

          return true;
        }

        // Remove focus state from element
        if (event == ftxui::Event::Escape) {
          // Invalidate old index for focused
          LOG("Handle menu navigation key=", util::EventToString(event));
          UpdateFocus(focus_index, kInvalidIndex);

          return true;
        }
      }

      return false;
    }

    /**
     * @brief Handles an event (from mouse)
     * @param event Received event from screen
     * @return true if event was handled, otherwise false
     */
    bool OnMouseEvent(const ftxui::Event& event) {
      // Iterate through all elements and pass event, if event is handled, update UI state
      bool event_handled =
          std::any_of(elements.begin(), elements.end(), [&event](Element* element) {
            if (!element) return false;
            return element->OnMouseEvent(event);
          });

      return event_handled;
    }

   private:
    /**
     * @brief Update focus state in both old and newly focused elements
     * @param old_index Element index with focus
     * @param new_index Element index to be focused
     */
    void UpdateFocus(int old_index, int new_index) {
      // If equal, do nothing
      if (old_index == new_index) return;

      // Remove focus from old focused frequency bar
      if (old_index != kInvalidIndex) elements[old_index]->focused = false;

      // Set focus on newly-focused frequency bar
      if (new_index != kInvalidIndex) elements[new_index]->focused = true;

      // Update internal index
      focus_index = new_index;
    }

    /**
     * @brief Check if contains any element focused
     * @return True if element focused, otherwise false
     */
    bool IsElementFocused() const { return focus_index != kInvalidIndex; }
  };

  /* ******************************************************************************************** */

  struct FrequencyBar final : public Element {
    static constexpr int kMaxGainLength = 8;  //!< Maximum string length in the input box for gain

    //! Style for frequency bar
    struct BarStyle {
      ftxui::Color background;
      ftxui::Color foreground;
    };

    //!< Color styles
    BarStyle style_normal = BarStyle{.background = ftxui::Color::LightSteelBlue3,
                                     .foreground = ftxui::Color::SteelBlue3};  //!< Normal mode

    BarStyle style_hovered = BarStyle{.background = ftxui::Color::LightSteelBlue1,
                                      .foreground = ftxui::Color::SlateBlue1};  //!< On hover state

    BarStyle style_focused = BarStyle{.background = ftxui::Color::LightSteelBlue3,
                                      .foreground = ftxui::Color::RedLight};  //!< On focus state

    model::AudioFilter* filter;  //!< Audio frequency filters for equalization

    /**
     * @brief Render frequency bar
     * @return UI element
     */
    ftxui::Element Render() override {
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
      if (focused)
        style = &style_focused;
      else
        style = hovered ? &style_hovered : &style_normal;

      return ftxui::vbox({
          // title
          empty_line(),
          ftxui::text(filter->GetFrequency()) | ftxui::color(ftxui::Color::White) | ftxui::hcenter,
          empty_line(),

          // frequency gauge
          gen_slider(gain, *style) | ftxui::reflect(box),

          // gain input
          empty_line(),
          ftxui::text(filter->GetGain()) | ftxui::color(ftxui::Color::White) | ftxui::inverted |
              ftxui::hcenter | ftxui::size(WIDTH, EQUAL, kMaxGainLength),
          empty_line(),
      });
    }

   private:
    /**
     * @brief Handles a navigation key event (arrow keys or hjkl)
     * @param event Received event from screen
     */
    void HandleNavigationKey(const ftxui::Event& event) override {
      if (!filter->modifiable) return;

      // Increment value and update UI
      if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
        double gain = filter->gain + 1;
        filter->SetNormalizedGain(gain);
      }

      // Decrement value and update UI
      if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
        double gain = filter->gain - 1;
        filter->SetNormalizedGain(gain);
      }
    }

    /**
     * @brief Handles a mouse scroll wheel event
     * @param button Received button event from screen
     */
    void HandleWheel(ftxui::Mouse::Button button) override {
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

      if ((focused && entry_focused == 0) || title_hovered) {
        title |= ftxui::inverted;
      }

      entries.reserve(opened ? presets.size() + 2 : 1);  // presets + title + separator
      entries.push_back(ftxui::hbox({prefix, title}) | ftxui::reflect(boxes[0]));

      if (opened) {
        entries.push_back(ftxui::separator());

        // Note: +1 or -1 below are used to ignore the title index
        for (int i = 0; i < presets.size(); i++) {
          bool active = presets[i] == *preset_name;
          bool is_focused =
              (focused && i == (entry_focused - 1)) || (hovered && i == (entry_hovered - 1));
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
                 content | ftxui::center | ftxui::border | ftxui::reflect(box),
                 ftxui::filler(),
             }) |
             ftxui::color(ftxui::Color::White);
    }

   private:
    /**
     * @brief Handles a navigation key event (arrow keys or hjkl)
     * @param event Received event from screen
     */
    void HandleNavigationKey(const ftxui::Event& event) override {
      if (event == ftxui::Event::Character(' ') || event == ftxui::Event::Return) {
        // Open element
        if (!opened) {
          opened = true;
          return;
        }

        // Close element
        if (entry_focused == 0) {
          opened = false;
          return;
        }

        // Select a new preset
        int offset = entry_focused - 1;
        if (presets[offset] != *preset_name) {
          update_preset(presets[offset]);
        }
      }

      if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j') && opened) {
        entry_focused = entry_focused + (entry_focused < static_cast<int>(presets.size()) ? 1 : 0);
      }

      if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k') && opened) {
        entry_focused = entry_focused - (entry_focused > 0 ? 1 : 0);
      }
    }

    /**
     * @brief Handles a mouse scroll wheel event
     * @param button Received button event from screen
     */
    void HandleWheel(ftxui::Mouse::Button button) override {
      // Update index based on internal state (if focused or hovered)
      auto update_index = [this, &button](int& index) {
        if (button == ftxui::Mouse::WheelUp)
          index = index - (index > 0 ? 1 : 0);
        else if (button == ftxui::Mouse::WheelDown) {
          index = index + (index < static_cast<int>(presets.size()) ? 1 : 0);
        }
      };

      if (opened) {
        update_index(focused ? entry_focused : entry_hovered);
      }
    }

    /**
     * @brief Handles a mouse click event
     * @param event Received event from screen
     */
    void HandleClick(ftxui::Event& event) final {
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
    void Update(const model::MusicGenre& genre_updated,
                const model::EqualizerPreset& preset_updated) {
      genre = genre_updated;
      preset = preset_updated;
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
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_
