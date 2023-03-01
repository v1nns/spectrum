#include "view/element/frequency_bar.h"

#include <algorithm>
#include <cmath>

namespace interface {

FrequencyBar::FrequencyBar(const model::AudioFilter& filter)
    : style_normal_{BarStyle{
          .background = ftxui::Color::LightSteelBlue3,
          .foreground = ftxui::Color::SteelBlue3,
      }},
      style_hovered_{BarStyle{
          .background = ftxui::Color::LightSteelBlue1,
          .foreground = ftxui::Color::SlateBlue1,
      }},
      box_{},
      hovered_{false},
      clicked_{false},
      focused_{false},
      filter_bar_{filter} {}

/* ********************************************************************************************** */

ftxui::Element FrequencyBar::Render() {
  using ftxui::WIDTH, ftxui::EQUAL;

  auto empty_line = []() { return ftxui::text(""); };

  auto gen_slider = [&](double value) {
    ftxui::Decorator color =
        hovered_
            ? ftxui::bgcolor(style_hovered_.background) | ftxui::color(style_hovered_.foreground)
            : ftxui::bgcolor(style_normal_.background) | ftxui::color(style_normal_.foreground);

    return ftxui::gaugeUp(value) | ftxui::yflex_grow | color;
  };

  float gain = filter_bar_.GetGainAsPercentage();

  return ftxui::vbox({
      // title
      empty_line(),
      ftxui::text(filter_bar_.GetFrequency()) | ftxui::hcenter,
      empty_line(),

      // frequency gauge
      // TODO: improve focused handling and styling
      ftxui::hbox({gen_slider(gain), gen_slider(gain)}) |
          (focused_ ? ftxui::border : ftxui::nothing) | ftxui::hcenter | ftxui::yflex_grow |
          ftxui::reflect(box_),

      // gain input
      empty_line(),
      ftxui::text(filter_bar_.GetGain()) | ftxui::inverted | ftxui::hcenter |
          ftxui::size(WIDTH, EQUAL, kMaxGainLength),
      empty_line(),
  });
}

/* ********************************************************************************************** */

bool FrequencyBar::OnEvent(ftxui::Event event) {
  if (event.mouse().button == ftxui::Mouse::WheelDown ||
      event.mouse().button == ftxui::Mouse::WheelUp) {
    if (hovered_) {
      double increment = event.mouse().button == ftxui::Mouse::WheelUp ? 1 : -1;
      filter_bar_.SetNormalizedGain(filter_bar_.gain + increment);

      return true;
    }

    return false;
  }

  if (event.mouse().button != ftxui::Mouse::None && event.mouse().button != ftxui::Mouse::Left) {
    return false;
  }

  if (box_.Contain(event.mouse().x, event.mouse().y)) {
    hovered_ = true;

    if (event.mouse().button == ftxui::Mouse::Left &&
        event.mouse().motion == ftxui::Mouse::Released) {
      // Calculate new value for gain based on coordinates from mouse click and bar size
      double value = std::ceil(model::AudioFilter::kMaxGain -
                               (event.mouse().y - box_.y_min) *
                                   (model::AudioFilter::kMaxGain - model::AudioFilter::kMinGain) /
                                   (box_.y_max - box_.y_min));

      filter_bar_.SetNormalizedGain(value);
      return true;
    }
  } else {
    hovered_ = false;
  }

  return false;
}

/* ********************************************************************************************** */

model::AudioFilter FrequencyBar::GetAudioFilter() const { return filter_bar_; }

/* ********************************************************************************************** */

void FrequencyBar::SetFocus() { focused_ = true; }

/* ********************************************************************************************** */

void FrequencyBar::IncreaseGain() {
  double value = filter_bar_.gain + 1;
  filter_bar_.SetNormalizedGain(value);
}

/* ********************************************************************************************** */

void FrequencyBar::DecreaseGain() {
  double value = filter_bar_.gain - 1;
  filter_bar_.SetNormalizedGain(value);
}

/* ********************************************************************************************** */

void FrequencyBar::ResetGain() { filter_bar_.gain = 0; }

/* ********************************************************************************************** */

void FrequencyBar::ResetFocus() { focused_ = false; }

}  // namespace interface
