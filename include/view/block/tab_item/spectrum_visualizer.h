/**
 * \file
 * \brief  Class for tab view containing spectrum visualizer
 */

#ifndef INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_VISUALIZER_H_
#define INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_VISUALIZER_H_

#include "model/bar_animation.h"
#include "view/element/tab_item.h"

namespace interface {

/**
 * @brief Component to render different animations using audio spectrum data from current song
 */
class SpectrumVisualizer : public TabItem {
  static constexpr int kGaugeThickness = 4;  //!< Gauge thickness + empty space
 public:
  /**
   * @brief Construct a new SpectrumVisualizer object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   */
  explicit SpectrumVisualizer(const model::BlockIdentifier& id,
                              const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the SpectrumVisualizer object
   */
  virtual ~SpectrumVisualizer() = default;

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
  bool OnEvent(ftxui::Event event) override;

  /**
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
  // Private methods
 private:
  // Gauge directions (not using all options from ftxui)
  enum class Direction { Up, Down };

  //! Utility to create UI gauge
  void CreateGauge(float value, Direction direction, ftxui::Elements& elements);

  //! Animations
  void DrawAnimationHorizontalMirror(ftxui::Element& visualizer);
  void DrawAnimationVerticalMirror(ftxui::Element& visualizer);
  void DrawAnimationMono(ftxui::Element& visualizer);

  /* ******************************************************************************************** */
  //! Variables
 private:
  model::BarAnimation curr_anim_;      //!< Control which bar animation to draw
  std::vector<double> spectrum_data_;  //!< Audio spectrum (each entry represents a frequency bar)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_VISUALIZER_H_