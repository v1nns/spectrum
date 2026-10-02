#include "view/block/main_content/spectrum_visualizer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <utility>
#include <vector>

#include "model/bar_animation.h"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/custom_event.h"
#include "view/base/keybinding.h"
#include "view/element/tab.h"

namespace interface {

SpectrumVisualizer::SpectrumVisualizer(const model::BlockIdentifier& id,
                                       const std::shared_ptr<EventDispatcher>& dispatcher,
                                       const FocusCallback& on_focus,
                                       const keybinding::Key& keybinding,
                                       const std::shared_ptr<util::FileHandler>& file_handler)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string{kTabName}),
      message_{[this] {
                 // Message has expired, so UI must be refreshed to remove it from screen
                 if (auto disp = dispatcher_.lock(); disp) {
                   disp->SendEvent(CustomEvent::Refresh());
                 }
               },
               kMessageDuration},
      file_handler_{file_handler} {
  // Restore settings from last run (ignoring invalid values)
  if (model::Settings settings; file_handler_ && file_handler_->ParseSettings(settings)) {
    if (settings.animation) curr_anim_ = *settings.animation;

    if (settings.bar_width && *settings.bar_width >= kGaugeMinWidth &&
        *settings.bar_width <= kGaugeMaxWidth) {
      gauge_width_ = *settings.bar_width;
    }

    LOG("Restored visualizer settings, animation=", model::GetAnimationName(curr_anim_),
        " bar width=", gauge_width_);
  }
}

/* ********************************************************************************************** */

ftxui::Element SpectrumVisualizer::Render() {
  ftxui::Element bar_visualizer = ftxui::emptyElement();

  switch (curr_anim_) {
    case model::BarAnimation::HorizontalMirror:
      DrawAnimationHorizontalMirror(bar_visualizer);
      break;

    case model::BarAnimation::VerticalMirror:
      DrawAnimationVerticalMirror(bar_visualizer);
      break;

    case model::BarAnimation::Mono:
      DrawAnimationMono(bar_visualizer);
      break;

    case model::BarAnimation::HorizontalMirrorNoSpace:
      DrawAnimationHorizontalMirror(bar_visualizer, false);
      break;

    case model::BarAnimation::VerticalMirrorNoSpace:
      DrawAnimationVerticalMirror(bar_visualizer, false);
      break;

    case model::BarAnimation::MonoNoSpace:
      DrawAnimationMono(bar_visualizer, false);
      break;

    case model::BarAnimation::SpectrumLine:
      DrawAnimationLine(bar_visualizer, LineStyle::Plain);
      break;

    case model::BarAnimation::SpectrumLineMirror:
      DrawAnimationLine(bar_visualizer, LineStyle::Mirror);
      break;

    case model::BarAnimation::SpectrumLineFilled:
      DrawAnimationLine(bar_visualizer, LineStyle::Filled);
      break;

    case model::BarAnimation::SpectrumLineFilledMirror:
      DrawAnimationLine(bar_visualizer, LineStyle::FilledMirror);
      break;

    case model::BarAnimation::LAST:
      ERROR("Audio visualizer current animation contains invalid value");
      curr_anim_ = model::BarAnimation::HorizontalMirror;
      break;
  }

  // Draw message (if any) on the upper-right corner, over the animation
  if (auto message = message_.GetText(); message.has_value()) {
    bar_visualizer = ftxui::dbox({
        bar_visualizer,
        ftxui::vbox({
            ftxui::hbox({
                ftxui::filler(),
                ftxui::text(*message) | ftxui::bold | ftxui::color(ftxui::Color::White),
            }),
            ftxui::filler(),
        }),
    });
  }

  // Draw picker (if open) on the upper-left corner, over the animation
  if (picker_previous_.has_value()) {
    bar_visualizer = ftxui::dbox({
        bar_visualizer,
        ftxui::hbox({RenderPicker(), ftxui::filler()}),
    });
  }

  return bar_visualizer;
}

/* ********************************************************************************************** */

ftxui::Element SpectrumVisualizer::RenderPicker() const {
  ftxui::Elements entries;

  for (int i = model::BarAnimation::HorizontalMirror; i < model::BarAnimation::LAST; i++) {
    const auto animation = static_cast<model::BarAnimation>(i);
    const bool selected = animation == curr_anim_;
    const std::string name{model::GetAnimationName(animation)};

    auto entry = ftxui::text((selected ? "▶ " : "  ") + name + " ");
    entries.push_back(selected
                          ? entry | ftxui::bold | ftxui::color(ftxui::Color::White) | ftxui::focus
                          : entry | ftxui::dim);
  }

  // Frame keeps selected entry visible when there is not enough space for all of them (otherwise,
  // picker takes only the height needed for its entries and border)
  const int max_height = static_cast<int>(entries.size()) + 2;

  return ftxui::vbox({
      ftxui::window(ftxui::text(" animation "),
                    ftxui::vbox(std::move(entries)) | ftxui::vscroll_indicator | ftxui::frame) |
          ftxui::clear_under | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, max_height),
      ftxui::filler(),
  });
}

/* ********************************************************************************************** */

void SpectrumVisualizer::ShowFullscreenHint() {
  const std::string key = util::EventToString(keybinding::Visualizer::ToggleFullscreen);
  message_.Show(key + ": exit fullscreen · " + std::string{model::GetAnimationName(curr_anim_)});
}

/* ********************************************************************************************** */

void SpectrumVisualizer::HideMessage() { message_.Hide(); }

/* ********************************************************************************************** */

bool SpectrumVisualizer::OnEvent(const ftxui::Event& event) {
  if (OnPickerEvent(event)) return true;

  // Enable/disable fullscreen mode with spectrum visualizer
  if (event == keybinding::Visualizer::ToggleFullscreen) {
    LOG("Handle key to toggle visualizer in fullscreen mode");
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    auto event_toggle = CustomEvent::ToggleFullscreen();
    dispatcher->SendEvent(event_toggle);

    return true;
  }

  // Increase/decrease bar width
  if (bool increase = event == keybinding::Visualizer::IncreaseBarWidth;
      event == keybinding::Visualizer::DecreaseBarWidth ||
      event == keybinding::Visualizer::IncreaseBarWidth) {
    LOG("Handle key to ", increase ? "increase" : "decrease", " audio bar width");
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    auto old_value = gauge_width_;

    if (increase && gauge_width_ < kGaugeMaxWidth) {
      gauge_width_++;
    } else if (!increase && gauge_width_ > kGaugeMinWidth) {
      gauge_width_--;
    }

    if (old_value != gauge_width_) {
      LOG("Changed audio bar width from ", old_value, " to ", gauge_width_);
      auto event_update = CustomEvent::UpdateBarWidth();
      dispatcher->SendEvent(event_update);

      SaveSettings();
      return true;
    }
  }

  return false;
}

/* ********************************************************************************************** */

bool SpectrumVisualizer::OnPickerEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  // Open picker, with current animation selected
  if (!picker_previous_.has_value()) {
    if (event != keybinding::Visualizer::ChangeAnimation) return false;

    LOG("Handle key to open animation picker");
    picker_previous_ = curr_anim_;
    message_.Hide();
    return true;
  }

  // Move selection, changing animation right away (so user can see it while choosing)
  if (bool next = event == Keybind::ArrowDown || event == Keybind::Down;
      next || event == Keybind::ArrowUp || event == Keybind::Up) {
    const int index = curr_anim_ + (next ? 1 : -1);

    if (index >= model::BarAnimation::HorizontalMirror && index < model::BarAnimation::LAST) {
      SetAnimation(static_cast<model::BarAnimation>(index));
    }

    return true;
  }

  // Keep selected animation
  if (event == Keybind::Return || event == keybinding::Visualizer::ChangeAnimation) {
    LOG("Selected animation=", model::GetAnimationName(curr_anim_));
    picker_previous_.reset();
    SaveSettings();
    return true;
  }

  // Go back to the animation from before opening picker
  if (event == Keybind::Escape) {
    LOG("Cancel animation picker");
    SetAnimation(*picker_previous_);
    picker_previous_.reset();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::SetAnimation(model::BarAnimation animation) {
  if (animation == curr_anim_) return;

  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  LOG("Change audio animation to ", model::GetAnimationName(animation));
  spectrum_data_.clear();
  curr_anim_ = animation;

  // Notify terminal to recalculate new size for spectrum data
  dispatcher->SendEvent(CustomEvent::ChangeBarAnimation(curr_anim_));
}

/* ********************************************************************************************** */

void SpectrumVisualizer::SaveSettings() const {
  if (!file_handler_) return;

  model::Settings settings{.animation = curr_anim_, .bar_width = gauge_width_};
  if (!file_handler_->SaveSettings(settings)) ERROR("Cannot save visualizer settings");
}

/* ********************************************************************************************** */

bool SpectrumVisualizer::OnCustomEvent(const CustomEvent& event) {
  // Store spectrum audio data to render later
  if (event == CustomEvent::Identifier::DrawAudioSpectrum) {
    spectrum_data_ = event.GetContent<std::vector<double>>();
    return true;
  }

  // Calculate new number of bars based on current animation
  if (event == CustomEvent::Identifier::CalculateNumberOfBars) {
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    int number_bars = event.GetContent<int>();

    // To fill entire screen, multiply value by 2
    if (model::IsAnimationFullWidthPerChannel(curr_anim_)) {
      number_bars *= 2;
    }

    auto event_resize = CustomEvent::ResizeAnalysis(number_bars);
    dispatcher->SendEvent(event_resize);

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::CreateGauge(double value, ftxui::Direction direction,
                                     ftxui::Elements& elements, bool space) const {
  using ftxui::gaugeDirection;
  constexpr auto color = [](const ftxui::Direction& dir) {
    auto gradient = ftxui::LinearGradient().Angle(dir == ftxui::Direction::Up ? 270 : 90);
    for (const auto& stop : kGradient) {
      gradient.Stop(ftxui::Color(stop.red, stop.green, stop.blue), stop.position);
    }

    return ftxui::color(gradient);
  };

  for (int i = 0; i < gauge_width_; i++) {
    elements.emplace_back(gaugeDirection(static_cast<float>(value), direction) | color(direction));
  }

  if (space) elements.emplace_back(ftxui::text(std::string(kGaugeSpacing, ' ')));
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationHorizontalMirror(ftxui::Element& visualizer, bool space) {
  auto size = (int)spectrum_data_.size();
  if (size == 0) return;

  ftxui::Elements entries;

  // Preallocate memory
  int total_size = size * (gauge_width_ + (space ? kGaugeSpacing : 0));
  entries.reserve(total_size);

  for (int i = (size / 2) - 1; i >= 0; i--) {
    CreateGauge(spectrum_data_[i], ftxui::Direction::Up, entries, space);
  }

  for (int i = size / 2; i < size; i++) {
    CreateGauge(spectrum_data_[i], ftxui::Direction::Up, entries, space);
  }

  if (space) {
    // Remove last empty space
    entries.pop_back();
  }

  visualizer = ftxui::hbox(entries) | ftxui::hcenter;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationVerticalMirror(ftxui::Element& visualizer, bool space) {
  auto size = (int)spectrum_data_.size();
  if (size == 0) return;

  ftxui::Elements left;
  ftxui::Elements right;

  // Preallocate memory
  int total_size = (size / 2) * (gauge_width_ + (space ? kGaugeSpacing : 0));
  left.reserve(total_size);
  right.reserve(total_size);

  for (int i = 0; i < size / 2; i++) {
    CreateGauge(spectrum_data_[i], ftxui::Direction::Up, left, space);
  }

  for (int i = size / 2; i < size; i++) {
    CreateGauge(spectrum_data_[i], ftxui::Direction::Down, right, space);
  }

  if (space) {
    // Remove last empty space
    left.pop_back();
    right.pop_back();
  }

  visualizer = ftxui::vbox(ftxui::hbox(left) | ftxui::hcenter | ftxui::yflex,
                           ftxui::hbox(right) | ftxui::hcenter | ftxui::yflex);
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationMono(ftxui::Element& visualizer, bool space) {
  auto size = (int)spectrum_data_.size();
  if (size == 0) return;

  // As total size is equal to the sum of both channels, this animation is the average of both
  // channels, so divide size by 2
  size /= 2;

  // Split data by channel
  std::vector<double>::const_iterator first = spectrum_data_.begin();
  std::vector<double>::const_iterator middle = spectrum_data_.begin() + size;
  std::vector<double>::const_iterator last = spectrum_data_.end();

  std::vector<double> left(first, middle);
  std::vector<double> right(middle, last);
  std::vector<double> average(size, 0);

  // Get average of each frequency from channels
  std::transform(left.begin(), left.end(),  // Left channel
                 right.begin(),             // Right channel
                 average.begin(),           // Average of the sum from both
                 [](double a, double b) { return (a + b) / 2; });

  ftxui::Elements entries;

  // Preallocate memory
  int total_size = size * (gauge_width_ + (space ? kGaugeSpacing : 0));
  entries.reserve(total_size);

  for (int i = 0; i < size; i++) {
    CreateGauge(average.at(i), ftxui::Direction::Up, entries, space);
  }

  if (space) {
    // Remove last empty space
    entries.pop_back();
  }

  visualizer = ftxui::hbox(entries) | ftxui::hcenter;
}

/* ********************************************************************************************** */

namespace {

//! Get spectrum value for a column of dots, interpolating between the nearest points
double ValueAt(const std::vector<double>& data, int x, int width) {
  const int points = static_cast<int>(data.size());
  const double position = static_cast<double>(x) * (points - 1) / (width - 1);
  const int index = std::min(static_cast<int>(position), points - 2);
  const double fraction = position - index;

  return (data.at(index) * (1 - fraction)) + (data.at(index + 1) * fraction);
}

}  // namespace

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationLine(ftxui::Element& visualizer, LineStyle style) {
  // Data contains both channels (left channel in the first half and right channel in the second)
  const auto size = static_cast<std::ptrdiff_t>(spectrum_data_.size() / 2);
  if (size < 2) {
    return;
  }

  std::vector<double> left(spectrum_data_.begin(), spectrum_data_.begin() + size);
  std::vector<double> right(spectrum_data_.begin() + size, spectrum_data_.begin() + (2 * size));

  // Single line uses the average from both channels
  if (style == LineStyle::Plain || style == LineStyle::Filled) {
    std::transform(left.begin(), left.end(), right.begin(), left.begin(),
                   [](double a, double b) { return (a + b) / 2; });
  }

  // Canvas is created with the size of the area available to it (in braille dots)
  auto draw = [left = std::move(left), right = std::move(right), style](ftxui::Canvas& canvas) {
    if (canvas.width() < 2 || canvas.height() < 2) {
      return;
    }

    switch (style) {
      case LineStyle::Plain:
        DrawLine(canvas, left);
        break;
      case LineStyle::Mirror:
        DrawMirroredLines(canvas, left, right);
        break;
      case LineStyle::Filled:
        DrawFilledArea(canvas, left);
        break;
      case LineStyle::FilledMirror:
        DrawMirroredFilledAreas(canvas, left, right);
        break;
    }
  };

  // Minimum size is a single cell, as canvas fills all the available space
  visualizer = ftxui::canvas(1, 1, std::move(draw)) | ftxui::flex;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawRun(ftxui::Canvas& canvas, const VerticalRun& run,
                                 const Baseline& baseline) {
  for (int y = std::min(run.from, run.to); y <= std::max(run.from, run.to); y++) {
    const double position = static_cast<double>(std::abs(baseline.y - y)) / baseline.range;
    canvas.DrawPoint(run.x, y, true, GetGradientColor(position));
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawLine(ftxui::Canvas& canvas, const std::vector<double>& data) {
  const int bottom = canvas.height() - 1;
  int previous = -1;

  for (int x = 0; x < canvas.width(); x++) {
    const int y = bottom - static_cast<int>(std::lround(ValueAt(data, x, canvas.width()) * bottom));

    // Connect to the previous column, so line keeps continuous even when it is steep
    DrawRun(canvas, VerticalRun{.x = x, .from = previous < 0 ? y : previous, .to = y},
            Baseline{.y = bottom, .range = bottom});

    previous = y;
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawMirroredLines(ftxui::Canvas& canvas, const std::vector<double>& left,
                                           const std::vector<double>& right) {
  const int bottom = canvas.height() - 1;
  const int middle = canvas.height() / 2;

  int previous_top = -1;
  int previous_down = -1;

  for (int x = 0; x < canvas.width(); x++) {
    // Left channel goes up from the middle, right channel goes down from it
    const int top =
        middle - static_cast<int>(std::lround(ValueAt(left, x, canvas.width()) * middle));
    const int down =
        middle +
        static_cast<int>(std::lround(ValueAt(right, x, canvas.width()) * (bottom - middle)));

    DrawRun(canvas, VerticalRun{.x = x, .from = previous_top < 0 ? top : previous_top, .to = top},
            Baseline{.y = middle, .range = middle});

    DrawRun(canvas,
            VerticalRun{.x = x, .from = previous_down < 0 ? down : previous_down, .to = down},
            Baseline{.y = middle, .range = bottom - middle});

    previous_top = top;
    previous_down = down;
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawFilledArea(ftxui::Canvas& canvas, const std::vector<double>& data) {
  const int bottom = canvas.height() - 1;

  for (int x = 0; x < canvas.width(); x++) {
    const int y = bottom - static_cast<int>(std::lround(ValueAt(data, x, canvas.width()) * bottom));

    // Fill everything below the line
    FillBlocks(canvas, VerticalRun{.x = x, .from = y, .to = bottom},
               Baseline{.y = bottom, .range = bottom});
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawMirroredFilledAreas(ftxui::Canvas& canvas,
                                                 const std::vector<double>& left,
                                                 const std::vector<double>& right) {
  const int bottom = canvas.height() - 1;
  const int middle = canvas.height() / 2;

  for (int x = 0; x < canvas.width(); x++) {
    // Left channel is filled up from the middle, right channel is filled down from it
    const int top =
        middle - static_cast<int>(std::lround(ValueAt(left, x, canvas.width()) * middle));
    const int down =
        middle +
        static_cast<int>(std::lround(ValueAt(right, x, canvas.width()) * (bottom - middle)));

    FillBlocks(canvas, VerticalRun{.x = x, .from = top, .to = middle},
               Baseline{.y = middle, .range = middle});
    FillBlocks(canvas, VerticalRun{.x = x, .from = middle, .to = down},
               Baseline{.y = middle, .range = bottom - middle});
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::FillBlocks(ftxui::Canvas& canvas, const VerticalRun& run,
                                    const Baseline& baseline) {
  // Blocks are used (instead of braille dots), so the area looks solid. Each block has the height
  // of two braille dots, so it must start from a coordinate multiple of its height
  static constexpr int kBlockHeight = 2;

  const int first = std::min(run.from, run.to);
  const int last = std::max(run.from, run.to);

  for (int block = first - (first % kBlockHeight); block <= last; block += kBlockHeight) {
    const double position = static_cast<double>(std::abs(baseline.y - block)) / baseline.range;
    canvas.DrawBlock(run.x, block, true, GetGradientColor(position));
  }
}

/* ********************************************************************************************** */

ftxui::Color SpectrumVisualizer::GetGradientColor(double position) {
  const double clamped = std::clamp(position, 0.0, 1.0);

  // Before first stop, there is nothing to interpolate
  if (clamped <= kGradient.front().position) {
    const auto& stop = kGradient.front();
    return {stop.red, stop.green, stop.blue};
  }

  for (size_t i = 1; i < kGradient.size(); i++) {
    const auto& start = kGradient.at(i - 1);
    const auto& end = kGradient.at(i);

    if (clamped <= end.position) {
      const double t = (clamped - start.position) / (end.position - start.position);
      auto lerp = [t](uint8_t a, uint8_t b) {
        return static_cast<uint8_t>(std::lround(a + ((b - a) * t)));
      };

      return {lerp(start.red, end.red), lerp(start.green, end.green), lerp(start.blue, end.blue)};
    }
  }

  // After last stop, there is nothing to interpolate
  const auto& stop = kGradient.back();
  return {stop.red, stop.green, stop.blue};
}

}  // namespace interface
