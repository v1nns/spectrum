#include "view/block/main_content/spectrum_visualizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "model/bar_animation.h"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/custom_event.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"
#include "view/element/tab.h"

namespace interface {

namespace {

/**
 * @brief Vertical gauge filled from top to bottom. Unlike ftxui::gaugeDirection with
 * ftxui::Direction::Down, the empty part is not drawn as full blocks with inverted colors, which
 * would look filled once dim is applied over it (e.g. while a dialog is opened). Its last cell is
 * filled from top by inverting colors from a block filling it from bottom, unless it is dimmed:
 * dim does not apply to inverted colors, so it uses the closest block filling a cell from top
 * instead (there are only a few of them)
 */
class DownGauge : public ftxui::Node {
  //! Blocks filling a cell from its bottom, from empty (none) to full (eight eighths)
  static constexpr std::array<std::string_view, 9> kLowerBlocks{" ", "▁", "▂", "▃", "▄",
                                                                "▅", "▆", "▇", "█"};
  static constexpr int kEighths = 8;  //!< Steps to fill a single cell

  //! Closest block filling a cell from its top, for each eighth filled (from none to all of them)
  static constexpr std::array<std::string_view, 9> kClosestUpperBlocks{" ", "▔", "▔", "▀", "▀",
                                                                       "▀", "▀", "█", "█"};

 public:
  DownGauge(float progress, bool dimmed)
      : progress_{std::clamp(progress, 0.F, 1.F)}, dimmed_{dimmed} {}

  void ComputeRequirement() override {
    requirement_.flex_grow_y = 1;
    requirement_.flex_shrink_y = 1;
    requirement_.min_x = 1;
    requirement_.min_y = 1;
  }

  void Render(ftxui::Screen& screen) override {
    const float limit = static_cast<float>(box_.y_min) +
                        progress_ * static_cast<float>(box_.y_max - box_.y_min + 1);
    const int full = static_cast<int>(limit);
    const int eighths = static_cast<int>(kEighths * (limit - static_cast<float>(full)));

    for (int x = box_.x_min; x <= box_.x_max; x++) {
      for (int y = box_.y_min; y <= box_.y_max; y++) {
        auto& cell = screen.CellAt(x, y);

        if (y < full) {
          cell.character = kLowerBlocks.back();
        } else if (y == full && eighths > 0 && dimmed_) {
          cell.character = kClosestUpperBlocks[eighths];
        } else if (y == full && eighths > 0) {
          // Only a partially filled cell needs inverted colors, so its top part is filled
          cell.character = kLowerBlocks[kEighths - eighths];
          cell.inverted = true;
        } else {
          cell.character = kLowerBlocks.front();
        }
      }
    }
  }

 private:
  float progress_;  //!< Filled part, from 0.0 to 1.0
  bool dimmed_;     //!< Dim is applied over gauge
};

/**
 * @brief Stack two elements vertically, giving both of them the same height. Unlike ftxui::vbox
 * with both elements flexible, the row left over from an odd height is not given to the bottom
 * one (which would make it look bigger), but kept empty at the top
 */
class EqualHalves : public ftxui::Node {
 public:
  EqualHalves(ftxui::Element top, ftxui::Element bottom)
      : ftxui::Node{{std::move(top), std::move(bottom)}} {}

  void ComputeRequirement() override {
    ftxui::Node::ComputeRequirement();

    const auto& top = children_.front()->requirement();
    const auto& bottom = children_.back()->requirement();

    requirement_.min_x = std::max(top.min_x, bottom.min_x);
    requirement_.min_y = 2 * std::max(top.min_y, bottom.min_y);
    requirement_.flex_grow_x = std::max(top.flex_grow_x, bottom.flex_grow_x);
    requirement_.flex_shrink_x = std::max(top.flex_shrink_x, bottom.flex_shrink_x);
    requirement_.flex_grow_y = 1;
    requirement_.flex_shrink_y = 1;
  }

  void SetBox(ftxui::Box box) override {
    ftxui::Node::SetBox(box);

    const int half = (box.y_max - box.y_min + 1) / 2;

    ftxui::Box bottom = box;
    bottom.y_min = box.y_max - half + 1;

    ftxui::Box top = box;
    top.y_max = bottom.y_min - 1;
    top.y_min = top.y_max - half + 1;

    children_.front()->SetBox(top);
    children_.back()->SetBox(bottom);
  }
};

/**
 * @brief Colors from a vertical gradient for each row of bars with the same height. FTXUI computes
 * gradient color for every cell (with gamma correction, which is expensive), but all bars have the
 * same height, so each row gets the same color on every bar. Colors are taken from FTXUI gradient
 * itself, so they look exactly the same
 */
class GradientRows {
 public:
  explicit GradientRows(ftxui::LinearGradient gradient) : gradient_{std::move(gradient)} {}

  //! Get color for each row of a bar with the given height (computed only when height changes)
  const std::vector<ftxui::Color>& Get(int height) {
    if (static_cast<int>(colors_.size()) != height) {
      ftxui::Screen column(1, height);
      ftxui::Render(column, ftxui::emptyElement() | ftxui::flex | ftxui::color(gradient_));

      colors_.clear();
      for (int y = 0; y < height; y++) colors_.push_back(column.CellAt(0, y).foreground_color);
    }

    return colors_;
  }

 private:
  ftxui::LinearGradient gradient_;    //!< Gradient to take colors from
  std::vector<ftxui::Color> colors_;  //!< Color for each row
};

/**
 * @brief Node that sets foreground color from a vertical gradient on every cell from its child,
 * same as ftxui::color with a ftxui::LinearGradient, but reusing colors computed for each row
 */
class GradientColor : public ftxui::Node {
 public:
  GradientColor(ftxui::Element child, std::shared_ptr<GradientRows> rows)
      : ftxui::Node({std::move(child)}), rows_{std::move(rows)} {}

  void SetBox(ftxui::Box box) override {
    ftxui::Node::SetBox(box);
    children_[0]->SetBox(box);
  }

  void Render(ftxui::Screen& screen) override {
    const auto& colors = rows_->Get(box_.y_max - box_.y_min + 1);

    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      const ftxui::Color& color = colors[static_cast<std::size_t>(y - box_.y_min)];
      for (int x = box_.x_min; x <= box_.x_max; ++x) screen.CellAt(x, y).foreground_color = color;
    }

    ftxui::Node::Render(screen);
  }

 private:
  std::shared_ptr<GradientRows> rows_;  //!< Colors for each row (shared by all bars)
};

/**
 * @brief Node that sets foreground color on each row from its child, with the color returned by a
 * function (for colors that depend on the bar itself, like a gradient squeezed into its height)
 */
class RowColor : public ftxui::Node {
 public:
  //! Get color for a row (counted from the base of bar) from a bar with the given height
  using ColorFunction = std::function<ftxui::Color(int row, int height)>;

  RowColor(ftxui::Element child, ftxui::Direction direction, ColorFunction function)
      : ftxui::Node({std::move(child)}), direction_{direction}, function_{std::move(function)} {}

  void SetBox(ftxui::Box box) override {
    ftxui::Node::SetBox(box);
    children_[0]->SetBox(box);
  }

  void Render(ftxui::Screen& screen) override {
    const int height = box_.y_max - box_.y_min + 1;

    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      const int row = direction_ == ftxui::Direction::Up ? box_.y_max - y : y - box_.y_min;
      const ftxui::Color color = function_(row, height);

      for (int x = box_.x_min; x <= box_.x_max; ++x) screen.CellAt(x, y).foreground_color = color;
    }

    ftxui::Node::Render(screen);
  }

 private:
  ftxui::Direction direction_;  //!< Direction where bar is filled (where its base is)
  ColorFunction function_;      //!< Color for each row
};

/**
 * @brief Draw a thin cap over the child gauge, on the recent maximum from bar (only when it is
 * above the bar itself)
 */
class PeakCap : public ftxui::Node {
 public:
  PeakCap(ftxui::Element gauge, double value, double peak, ftxui::Direction direction)
      : ftxui::Node({std::move(gauge)}), value_{value}, peak_{peak}, direction_{direction} {}

  void SetBox(ftxui::Box box) override {
    ftxui::Node::SetBox(box);
    children_[0]->SetBox(box);
  }

  void Render(ftxui::Screen& screen) override {
    ftxui::Node::Render(screen);

    const int height = box_.y_max - box_.y_min + 1;
    const double top = std::clamp(peak_, 0.0, 1.0) * height;

    // Cell where cap is (the one whose upper edge is at peak or above it), and cells occupied by
    // bar itself (even partially filled ones)
    const int cell = std::max(0, static_cast<int>(std::ceil(top)) - 1);
    const int occupied = static_cast<int>(std::ceil(std::clamp(value_, 0.0, 1.0) * height));
    if (cell < occupied) return;

    // Cap is placed on the half of the cell closer to its value
    const bool upper_half = top - cell > 0.5;
    const bool up = direction_ == ftxui::Direction::Up;
    const int y = up ? box_.y_max - cell : box_.y_min + cell;

    for (int x = box_.x_min; x <= box_.x_max; ++x) {
      screen.CellAt(x, y).character = upper_half == up ? "▔" : "▁";
    }
  }

 private:
  double value_;                //!< Bar value
  double peak_;                 //!< Peak value
  ftxui::Direction direction_;  //!< Direction where bar is filled
};

/**
 * @brief Base for a single column from a bar drawn with a custom style, filling all the height
 * available to it
 */
class StyledBar : public ftxui::Node {
 public:
  StyledBar(double value, std::optional<double> peak, ftxui::Direction direction)
      : value_{std::clamp(value, 0.0, 1.0)},
        peak_{peak ? std::optional{std::clamp(*peak, 0.0, 1.0)} : std::nullopt},
        direction_{direction} {}

  void ComputeRequirement() override {
    requirement_.flex_grow_y = 1;
    requirement_.flex_shrink_y = 1;
    requirement_.min_x = 1;
    requirement_.min_y = 1;
  }

 protected:
  //! Get row on screen for the given row from bar (counted from its base)
  int GetY(int row) const {
    return direction_ == ftxui::Direction::Up ? box_.y_max - row : box_.y_min + row;
  }

  //! Get height available to bar
  int GetHeight() const { return box_.y_max - box_.y_min + 1; }

  //! Get the last step filled by bar and peak cap, from steps available (-1 means no peak cap, as
  //! it is shown only above bar)
  std::pair<int, int> GetFilled(int steps) const {
    const int filled = static_cast<int>(std::lround(value_ * steps));
    const int cap = peak_ ? static_cast<int>(std::lround(*peak_ * steps)) - 1 : -1;
    return {filled, cap >= filled ? cap : -1};
  }

  bool IsUp() const { return direction_ == ftxui::Direction::Up; }

 private:
  double value_;                //!< Filled part, from 0.0 to 1.0
  std::optional<double> peak_;  //!< Peak cap (if any)
  ftxui::Direction direction_;  //!< Direction where bar is filled
};

/**
 * @brief Bar made of separate segments, one for each cell, like a hardware equalizer
 */
class LedBar : public StyledBar {
 public:
  using StyledBar::StyledBar;

  void Render(ftxui::Screen& screen) override {
    // Half block leaves the other half of the cell empty, which separates segments
    const std::string segment = IsUp() ? "▄" : "▀";
    const auto [lit, cap] = GetFilled(GetHeight());

    for (int row = 0; row < GetHeight(); row++) {
      const bool on = row < lit || row == cap;
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, GetY(row)).character = on ? segment : " ";
      }
    }
  }
};

/**
 * @brief Bar made of braille dots, with four steps for each cell
 */
class BrailleBar : public StyledBar {
  static constexpr int kDotRows = 4;      //!< Rows of dots in a single cell
  static constexpr char32_t kBlank = 0x2800;  //!< Braille pattern without any dot

  //! Bits for each row of dots (both columns), from top to bottom
  static constexpr std::array<int, kDotRows> kRowBits{0x09, 0x12, 0x24, 0xC0};

 public:
  using StyledBar::StyledBar;

  void Render(ftxui::Screen& screen) override {
    const auto [filled, cap] = GetFilled(GetHeight() * kDotRows);

    for (int row = 0; row < GetHeight(); row++) {
      int bits = 0;

      for (int dot = 0; dot < kDotRows; dot++) {
        const int step = (row * kDotRows) + dot;
        if (step >= filled && step != cap) continue;

        // Dots from base of bar are the bottom ones when it is filled up, otherwise the top ones
        bits |= kRowBits.at(static_cast<size_t>(IsUp() ? kDotRows - 1 - dot : dot));
      }

      const std::string character = bits == 0 ? " " : ToUtf8(kBlank + static_cast<char32_t>(bits));
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        screen.CellAt(x, GetY(row)).character = character;
      }
    }
  }

 private:
  //! Encode a character from braille block (three bytes in UTF-8)
  static std::string ToUtf8(char32_t code) {
    static constexpr int kShift = 6;
    static constexpr char32_t kMask = 0x3F;

    return {static_cast<char>(0xE0 | (code >> (2 * kShift))),
            static_cast<char>(0x80 | ((code >> kShift) & kMask)),
            static_cast<char>(0x80 | (code & kMask))};
  }
};

//! Get spectrum value for a column, interpolating between the nearest points
double ValueAt(const std::vector<double>& data, int x, int width) {
  const int points = static_cast<int>(data.size());
  if (points == 1 || width < 2) return data.front();

  const double position = static_cast<double>(x) * (points - 1) / (width - 1);
  const int index = std::min(static_cast<int>(position), points - 2);
  const double fraction = position - index;

  return (data.at(index) * (1 - fraction)) + (data.at(index + 1) * fraction);
}

/**
 * @brief Spectrum from each moment as a row (the newest one on top), each cell shaded by its value
 */
class WaterfallRows : public ftxui::Node {
  //! Shades from the lowest value to the highest one
  static constexpr std::array<std::string_view, 5> kShades{" ", "░", "▒", "▓", "█"};

  //! Values up to this one are not drawn (otherwise, quiet noise fills the whole area)
  static constexpr double kFloor = 0.15;

 public:
  //! Get color for a value from the given column (from 0.0 to 1.0)
  using ColorFunction = std::function<ftxui::Color(double value, double column)>;

  WaterfallRows(const std::deque<std::vector<double>>& rows, ColorFunction function)
      : rows_{rows}, function_{std::move(function)} {}

  void ComputeRequirement() override {
    requirement_.flex_grow_x = 1;
    requirement_.flex_grow_y = 1;
    requirement_.flex_shrink_x = 1;
    requirement_.flex_shrink_y = 1;
    requirement_.min_x = 1;
    requirement_.min_y = 1;
  }

  void Render(ftxui::Screen& screen) override {
    const int width = box_.x_max - box_.x_min + 1;
    const int last = static_cast<int>(kShades.size()) - 1;

    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      const auto index = static_cast<size_t>(y - box_.y_min);
      if (index >= rows_.size()) break;

      const auto& row = rows_[index];
      if (row.empty()) continue;

      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        const double value = std::clamp(ValueAt(row, x - box_.x_min, width), 0.0, 1.0);
        const double above_floor = std::max(0.0, value - kFloor) / (1 - kFloor);
        const int shade = std::min(last, static_cast<int>(std::ceil(above_floor * last)));
        if (shade == 0) continue;

        auto& cell = screen.CellAt(x, y);
        cell.character = kShades.at(static_cast<size_t>(shade));
        cell.foreground_color = function_(value, width > 1 ? (x - box_.x_min) / (width - 1.0) : 0);
      }
    }
  }

 private:
  //! Rows to draw (kept by visualizer, which renders this node right after creating it)
  const std::deque<std::vector<double>>& rows_;
  ColorFunction function_;  //!< Color for each cell
};

/**
 * @brief Rotate hue from color and mix it with white (for hue cycle and beat flash)
 * @param stop Color stop from gradient
 * @param degrees Hue rotation
 * @param white Amount of white to mix, from 0.0 to 1.0
 * @return Modified color stop
 */
Theme::ColorStop Shift(const Theme::ColorStop& stop, double degrees, double white) {
  static constexpr double kMax = 255.0;
  static constexpr double kSextant = 60.0;
  static constexpr double kTurn = 360.0;

  const double r = stop.red / kMax;
  const double g = stop.green / kMax;
  const double b = stop.blue / kMax;

  // Convert to HSL
  const double max = std::max({r, g, b});
  const double min = std::min({r, g, b});
  const double lightness = (max + min) / 2;
  const double delta = max - min;

  double hue = 0;
  double saturation = 0;

  if (delta > 0) {
    saturation = delta / (1 - std::abs((2 * lightness) - 1));

    if (max == r) {
      hue = std::fmod((g - b) / delta, 6.0);
    } else if (max == g) {
      hue = ((b - r) / delta) + 2;
    } else {
      hue = ((r - g) / delta) + 4;
    }

    hue *= kSextant;
  }

  hue = std::fmod(hue + degrees + kTurn, kTurn);

  // Convert it back to RGB
  const double chroma = (1 - std::abs((2 * lightness) - 1)) * saturation;
  const double second = chroma * (1 - std::abs(std::fmod(hue / kSextant, 2.0) - 1));
  const double offset = lightness - (chroma / 2);

  std::array<double, 3> rgb{};
  switch (static_cast<int>(hue / kSextant)) {
    case 0:
      rgb = {chroma, second, 0};
      break;
    case 1:
      rgb = {second, chroma, 0};
      break;
    case 2:
      rgb = {0, chroma, second};
      break;
    case 3:
      rgb = {0, second, chroma};
      break;
    case 4:
      rgb = {second, 0, chroma};
      break;
    default:
      rgb = {chroma, 0, second};
      break;
  }

  auto channel = [offset, white](double value) {
    const double mixed = value + offset + ((1 - (value + offset)) * white);
    return static_cast<uint8_t>(std::lround(std::clamp(mixed, 0.0, 1.0) * kMax));
  };

  return Theme::ColorStop{stop.position, channel(rgb[0]), channel(rgb[1]), channel(rgb[2])};
}

}  // namespace

/* ********************************************************************************************** */

/**
 * @brief Colors for a single frame, picking them from theme (gradient, single color or zones)
 * according to the chosen colors option
 */
struct SpectrumVisualizer::Colorizer {
  //! Limits for each zone, from the base of bars (the last zone goes up to the top)
  static constexpr std::array<double, 2> kZoneLimits{0.6, 0.85};

  model::BarColors mode;                   //!< How gradient is spread
  std::array<Theme::ColorStop, 4> stops;   //!< Gradient (with hue cycle already applied)
  std::optional<ftxui::Color> solid;       //!< Single color from theme (instead of gradient)
  std::array<ftxui::Color, 3> zones;       //!< Colors for zones
  std::shared_ptr<GradientRows> rows_up;   //!< Vertical gradient for bars filled up
  std::shared_ptr<GradientRows> rows_down; //!< Vertical gradient for bars filled down

  //! Get color from gradient
  //! @param position Position in gradient, from 0.0 (lowest) to 1.0 (highest)
  ftxui::Color Gradient(double position) const {
    if (solid) return *solid;

    const double clamped = std::clamp(position, 0.0, 1.0);

    // Before first stop, there is nothing to interpolate
    if (clamped <= stops.front().position) {
      const auto& stop = stops.front();
      return {stop.red, stop.green, stop.blue};
    }

    for (size_t i = 1; i < stops.size(); i++) {
      const auto& start = stops.at(i - 1);
      const auto& end = stops.at(i);

      if (clamped <= end.position) {
        const double t = (clamped - start.position) / (end.position - start.position);
        auto lerp = [t](uint8_t a, uint8_t b) {
          return static_cast<uint8_t>(std::lround(a + ((b - a) * t)));
        };

        return {lerp(start.red, end.red), lerp(start.green, end.green),
                lerp(start.blue, end.blue)};
      }
    }

    // After last stop, there is nothing to interpolate
    const auto& stop = stops.back();
    return {stop.red, stop.green, stop.blue};
  }

  //! Get color from gradient going across both directions
  //! @param position Distance from the base, from 0.0 to 1.0
  //! @param column Horizontal position, from 0.0 (left) to 1.0 (right)
  ftxui::Color Diagonal(double position, double column) const {
    return Gradient((position + column) / 2);
  }

  //! Get color for a zone
  //! @param position Distance from the base, from 0.0 to 1.0
  ftxui::Color Zone(double position) const {
    if (position < kZoneLimits[0]) return zones[0];
    if (position < kZoneLimits[1]) return zones[1];
    return zones[2];
  }

  /**
   * @brief Get color for a single point from spectrum
   * @param position Distance from the base, from 0.0 to 1.0
   * @param level Spectrum value where point is, from 0.0 to 1.0
   * @param column Horizontal position, from 0.0 (left) to 1.0 (right)
   * @return Color
   */
  ftxui::Color At(double position, double level, double column) const {
    switch (mode) {
      case model::BarColors::Level:
        return Gradient(level);
      case model::BarColors::LevelStretched:
        return Gradient(level > 0 ? position / level : 0);
      case model::BarColors::Horizontal:
        return Gradient(column);
      case model::BarColors::Diagonal:
        return Diagonal(position, column);
      case model::BarColors::Zones:
        return Zone(position);
      case model::BarColors::Vertical:
      case model::BarColors::LAST:
        break;
    }

    return Gradient(position);
  }

  /**
   * @brief Get color for a bar
   * @param value Bar value, from 0.0 to 1.0
   * @param column Bar position, from the first bar (0.0) to the last one (1.0)
   * @param direction Direction where bar is filled
   * @return Decorator setting color
   */
  ftxui::Decorator ForBar(double value, double column, ftxui::Direction direction) const {
    if (solid && mode != model::BarColors::Zones) return ftxui::color(*solid);

    switch (mode) {
      case model::BarColors::Level:
        return ftxui::color(Gradient(value));

      case model::BarColors::Horizontal:
        return ftxui::color(Gradient(column));

      case model::BarColors::LevelStretched:
        return [this_copy = *this, value, direction](ftxui::Element child) -> ftxui::Element {
          return std::make_shared<RowColor>(
              std::move(child), direction, [this_copy, value](int row, int height) {
                // Gradient goes from the base of bar to its last cell
                const double filled = std::max(1.0, std::ceil(value * height) - 1);
                return this_copy.Gradient(row / filled);
              });
        };

      case model::BarColors::Diagonal:
        return [this_copy = *this, column, direction](ftxui::Element child) -> ftxui::Element {
          return std::make_shared<RowColor>(
              std::move(child), direction, [this_copy, column](int row, int height) {
                // Gradient goes from the base of area to its top
                return this_copy.Diagonal(row / std::max(1.0, height - 1.0), column);
              });
        };

      case model::BarColors::Zones:
        return [this_copy = *this, direction](ftxui::Element child) -> ftxui::Element {
          return std::make_shared<RowColor>(std::move(child), direction,
                                            [this_copy](int row, int height) {
                                              return this_copy.Zone((row + 0.5) / height);
                                            });
        };

      case model::BarColors::Vertical:
      case model::BarColors::LAST:
        break;
    }

    // Shared by all bars, so colors are computed only once for each row
    auto rows = direction == ftxui::Direction::Up ? rows_up : rows_down;
    return [rows](ftxui::Element child) -> ftxui::Element {
      return std::make_shared<GradientColor>(std::move(child), rows);
    };
  }
};

/* ********************************************************************************************** */

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
      file_handler_{file_handler},
      picker_{[this](const model::VisualizerOptions& options) { SetOptions(options); },
              [this](const model::VisualizerOptions& options) {
                INFO("Selected visualizer options, animation=",
                     model::GetAnimationName(options.animation),
                     " bars=", model::GetName(options.bars), " spacing=", options.spacing,
                     " colors=", model::GetName(options.colors), " peaks=", options.peaks,
                     " hue=", model::GetName(options.hue));
                SetOptions(options);
                SaveSettings();
              }} {
  // Restore settings from last run (ignoring invalid values)
  if (model::Settings settings; file_handler_ && file_handler_->ParseSettings(settings)) {
    options_ = model::VisualizerOptions{
        .animation = settings.animation.value_or(options_.animation),
        .bars = settings.bar_style.value_or(options_.bars),
        .spacing = settings.bar_spacing.value_or(options_.spacing),
        .colors = settings.bar_colors.value_or(options_.colors),
        .peaks = settings.peaks.value_or(options_.peaks),
        .hue = settings.hue_cycle.value_or(options_.hue),
    };

    if (settings.bar_width && *settings.bar_width >= kGaugeMinWidth &&
        *settings.bar_width <= kGaugeMaxWidth) {
      gauge_width_ = *settings.bar_width;
    }

    INFO("Restored visualizer settings, animation=", model::GetAnimationName(options_.animation),
         " bar width=", gauge_width_);
  }
}

/* ********************************************************************************************** */

ftxui::Element SpectrumVisualizer::Render() {
  ftxui::Element bar_visualizer = ftxui::emptyElement();
  const Colorizer colorizer = GetColorizer();

  switch (options_.animation) {
    case model::BarAnimation::HorizontalMirror:
      DrawAnimationHorizontalMirror(bar_visualizer, colorizer);
      break;

    case model::BarAnimation::VerticalMirror:
      DrawAnimationVerticalMirror(bar_visualizer, colorizer);
      break;

    case model::BarAnimation::Mono:
      DrawAnimationMono(bar_visualizer, colorizer);
      break;

    case model::BarAnimation::SpectrumLine:
      DrawAnimationLine(bar_visualizer, LineStyle::Plain, colorizer);
      break;

    case model::BarAnimation::SpectrumLineMirror:
      DrawAnimationLine(bar_visualizer, LineStyle::Mirror, colorizer);
      break;

    case model::BarAnimation::SpectrumLineFilled:
      DrawAnimationLine(bar_visualizer, LineStyle::Filled, colorizer);
      break;

    case model::BarAnimation::SpectrumLineFilledMirror:
      DrawAnimationLine(bar_visualizer, LineStyle::FilledMirror, colorizer);
      break;

    case model::BarAnimation::Radial:
      DrawAnimationRadial(bar_visualizer, colorizer);
      break;

    case model::BarAnimation::Waterfall:
      DrawAnimationWaterfall(bar_visualizer, colorizer);
      break;

    case model::BarAnimation::LAST:
      ERROR("Audio visualizer current animation contains invalid value");
      options_.animation = model::BarAnimation::HorizontalMirror;
      break;
  }

  // Draw message (if any) on the upper-right corner, over the animation
  if (auto message = message_.GetText(); message.has_value()) {
    bar_visualizer = ftxui::dbox({
        bar_visualizer,
        ftxui::vbox({
            ftxui::hbox({
                ftxui::filler(),
                ftxui::text(*message) | ftxui::bold | ftxui::color(GetTheme().visualizer.text),
            }),
            ftxui::filler(),
        }),
    });
  }

  // Draw picker (if open) on the upper-left corner, over the animation
  if (picker_.IsVisible()) {
    bar_visualizer = ftxui::dbox({
        bar_visualizer,
        ftxui::hbox({ftxui::vbox({picker_.Render(), ftxui::filler()}), ftxui::filler()}),
    });
  }

  return bar_visualizer;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::ShowFullscreenHint() {
  const std::string key = util::EventToString(keybinding::Visualizer::ToggleFullscreen);
  message_.Show(key + ": exit fullscreen · " +
                std::string{model::GetAnimationName(options_.animation)});
}

/* ********************************************************************************************** */

void SpectrumVisualizer::HideMessage() { message_.Hide(); }

/* ********************************************************************************************** */

bool SpectrumVisualizer::OnEvent(const ftxui::Event& event) {
  // While picker is open, it is the first one to handle events
  if (picker_.OnEvent(event)) return true;

  // Open picker, with current options
  if (event == keybinding::Visualizer::ChangeAnimation) {
    LOG("Handle key to open visualizer picker");
    picker_.Open(options_);
    message_.Hide();
    return true;
  }

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

bool SpectrumVisualizer::OnMouseEvent(ftxui::Event& event) { return picker_.OnEvent(event); }

/* ********************************************************************************************** */

void SpectrumVisualizer::OnLostFocus() {
  if (picker_.IsVisible()) picker_.Cancel();
}

/* ********************************************************************************************** */

void SpectrumVisualizer::SetOptions(const model::VisualizerOptions& options) {
  if (options == options_) return;

  // Number of bars depends on animation and spacing, so terminal must recalculate it
  const bool layout_changed =
      options.animation != options_.animation || options.spacing != options_.spacing;

  auto dispatcher = dispatcher_.lock();
  if (layout_changed && !dispatcher) return;

  LOG("Change visualizer animation to ", model::GetAnimationName(options.animation));
  if (layout_changed) spectrum_data_.clear();
  if (options.animation != options_.animation) waterfall_.clear();
  peaks_.clear();

  options_ = options;

  if (layout_changed) dispatcher->SendEvent(CustomEvent::ChangeBarAnimation(options_.animation));
}

/* ********************************************************************************************** */

void SpectrumVisualizer::SaveSettings() const {
  if (!file_handler_) return;

  model::Settings settings{.bar_width = gauge_width_};

  // While picker is open, current options are only a preview (they may still be cancelled)
  if (!picker_.IsVisible()) {
    settings.animation = options_.animation;
    settings.bar_style = options_.bars;
    settings.bar_spacing = options_.spacing;
    settings.bar_colors = options_.colors;
    settings.peaks = options_.peaks;
    settings.hue_cycle = options_.hue;
  }

  if (!file_handler_->SaveSettings(settings)) ERROR("Cannot save visualizer settings");
}

/* ********************************************************************************************** */

bool SpectrumVisualizer::OnCustomEvent(const CustomEvent& event) {
  // Store spectrum audio data to render later
  if (event == CustomEvent::Identifier::DrawAudioSpectrum) {
    spectrum_data_ = event.GetContent<std::vector<double>>();
    UpdateEffects();
    return true;
  }

  // Number of bars was already calculated by Terminal based on current animation
  if (event == CustomEvent::Identifier::CalculateNumberOfBars) {
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    int number_bars = event.GetContent<int>();

    auto event_resize = CustomEvent::ResizeAnalysis(number_bars);
    dispatcher->SendEvent(event_resize);

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::UpdateEffects() {
  // Beat is detected when lowest frequencies are much louder than their recent average
  static constexpr double kBassPart = 8.0;        //!< Part of each channel with lowest frequencies
  static constexpr double kBeatThreshold = 0.25;  //!< Minimum value for a beat
  static constexpr double kBeatRatio = 1.3;       //!< Minimum ratio to recent average
  static constexpr int kBeatMinFrames = 10;       //!< Minimum frames between two beats
  static constexpr double kAverageWeight = 0.1;   //!< Weight of new frame on recent average
  static constexpr double kFlashDecay = 0.08;     //!< Flash reduced on each frame

  const size_t half = spectrum_data_.size() / 2;

  // Peak caps hold for a moment, then fall faster and faster until they reach bar again
  if (options_.peaks && model::IsBarAnimation(options_.animation)) {
    if (peaks_.size() != spectrum_data_.size()) peaks_.assign(spectrum_data_.size(), Peak{});

    for (size_t i = 0; i < spectrum_data_.size(); i++) {
      auto& peak = peaks_[i];
      const double value = spectrum_data_[i];

      if (value >= peak.value) {
        peak = Peak{.value = value, .hold = kPeakHoldFrames, .speed = 0};
      } else if (peak.hold > 0) {
        peak.hold--;
      } else {
        peak.speed += kPeakGravity;
        peak.value = std::max(value, peak.value - peak.speed);
      }
    }
  }

  if (options_.hue != model::HueCycle::Off) hue_ = std::fmod(hue_ + kHueStep, 360.0);

  if (options_.hue == model::HueCycle::Flash && half > 0) {
    const auto bass_size = static_cast<size_t>(std::max(1.0, std::ceil(half / kBassPart)));
    double bass = 0;

    for (size_t i = 0; i < bass_size; i++) bass += spectrum_data_[i] + spectrum_data_[half + i];
    bass /= static_cast<double>(2 * bass_size);

    if (bass > kBeatThreshold && bass > bass_average_ * kBeatRatio &&
        frames_since_beat_ >= kBeatMinFrames) {
      flash_ = 1;
      frames_since_beat_ = 0;
    } else {
      flash_ = std::max(0.0, flash_ - kFlashDecay);
      frames_since_beat_++;
    }

    bass_average_ = (bass_average_ * (1 - kAverageWeight)) + (bass * kAverageWeight);
  } else {
    flash_ = 0;
  }

  // Waterfall keeps the average from both channels, adding a row every few frames
  if (options_.animation == model::BarAnimation::Waterfall && half > 0 &&
      ++waterfall_frames_ >= kWaterfallFrameStep) {
    waterfall_frames_ = 0;

    std::vector<double> row(half);
    for (size_t i = 0; i < half; i++) row[i] = (spectrum_data_[i] + spectrum_data_[half + i]) / 2;

    waterfall_.push_front(std::move(row));
    if (waterfall_.size() > kWaterfallMaxRows) waterfall_.pop_back();
  }
}

/* ********************************************************************************************** */

SpectrumVisualizer::Colorizer SpectrumVisualizer::GetColorizer() const {
  // Brightness added by a beat flash (mixed with white)
  static constexpr double kFlashWhite = 0.45;

  const auto& theme = GetTheme().visualizer;

  Colorizer colorizer{
      .mode = options_.colors,
      .stops = theme.gradient,
      .solid = theme.solid,
      .zones = theme.zones,
      .rows_up = nullptr,
      .rows_down = nullptr,
  };

  if (options_.hue != model::HueCycle::Off && !theme.solid) {
    const double white = flash_ * kFlashWhite;
    std::transform(colorizer.stops.begin(), colorizer.stops.end(), colorizer.stops.begin(),
                   [this, white](const Theme::ColorStop& stop) { return Shift(stop, hue_, white); });
  }

  // Colors from vertical gradient are taken from FTXUI itself (and computed lazily)
  if (!theme.solid && options_.colors == model::BarColors::Vertical) {
    auto rows = [&colorizer](int angle) {
      auto gradient = ftxui::LinearGradient().Angle(static_cast<float>(angle));
      for (const auto& stop : colorizer.stops) {
        gradient.Stop(ftxui::Color(stop.red, stop.green, stop.blue), stop.position);
      }
      return std::make_shared<GradientRows>(std::move(gradient));
    };

    static constexpr int kUp = 270;
    static constexpr int kDown = 90;

    colorizer.rows_up = rows(kUp);
    colorizer.rows_down = rows(kDown);
  }

  return colorizer;
}

/* ********************************************************************************************** */

std::optional<double> SpectrumVisualizer::GetPeak(size_t index) const {
  if (!options_.peaks || index >= peaks_.size()) return std::nullopt;
  return peaks_[index].value;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::CreateGauge(double value, std::optional<double> peak, double column,
                                     ftxui::Direction direction, const Colorizer& colorizer,
                                     ftxui::Elements& elements) const {
  using ftxui::gaugeDirection;

  const ftxui::Decorator color = colorizer.ForBar(value, column, direction);

  for (int i = 0; i < gauge_width_; i++) {
    ftxui::Element gauge;

    switch (options_.bars) {
      case model::BarStyle::Led:
        gauge = std::make_shared<LedBar>(value, peak, direction);
        break;

      case model::BarStyle::Braille:
        gauge = std::make_shared<BrailleBar>(value, peak, direction);
        break;

      case model::BarStyle::Solid:
      case model::BarStyle::LAST:
        gauge = direction == ftxui::Direction::Down
                    ? std::make_shared<DownGauge>(static_cast<float>(value), dimmed_)
                    : gaugeDirection(static_cast<float>(value), direction);

        if (peak) gauge = std::make_shared<PeakCap>(std::move(gauge), value, *peak, direction);
        break;
    }

    elements.emplace_back(std::move(gauge) | color);
  }

  if (options_.spacing) elements.emplace_back(ftxui::text(std::string(kGaugeSpacing, ' ')));
}

/* ********************************************************************************************** */

namespace {

//! Get position from a bar, from the first one (0.0) to the last one (1.0)
double Column(int index, int count) {
  return count > 1 ? static_cast<double>(index) / (count - 1) : 0;
}

}  // namespace

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationHorizontalMirror(ftxui::Element& visualizer,
                                                       const Colorizer& colorizer) {
  auto size = (int)spectrum_data_.size();
  if (size == 0) return;

  ftxui::Elements entries;

  // Preallocate memory
  int total_size = size * (gauge_width_ + (options_.spacing ? kGaugeSpacing : 0));
  entries.reserve(total_size);

  int column = 0;

  for (int i = (size / 2) - 1; i >= 0; i--) {
    CreateGauge(spectrum_data_[i], GetPeak(i), Column(column++, size), ftxui::Direction::Up,
                colorizer, entries);
  }

  for (int i = size / 2; i < size; i++) {
    CreateGauge(spectrum_data_[i], GetPeak(i), Column(column++, size), ftxui::Direction::Up,
                colorizer, entries);
  }

  if (options_.spacing) {
    // Remove last empty space
    entries.pop_back();
  }

  visualizer = ftxui::hbox(entries) | ftxui::hcenter;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationVerticalMirror(ftxui::Element& visualizer,
                                                     const Colorizer& colorizer) {
  auto size = (int)spectrum_data_.size();
  if (size == 0) return;

  ftxui::Elements left;
  ftxui::Elements right;

  // Preallocate memory
  int total_size = (size / 2) * (gauge_width_ + (options_.spacing ? kGaugeSpacing : 0));
  left.reserve(total_size);
  right.reserve(total_size);

  const int half = size / 2;

  for (int i = 0; i < half; i++) {
    CreateGauge(spectrum_data_[i], GetPeak(i), Column(i, half), ftxui::Direction::Up, colorizer,
                left);
  }

  for (int i = half; i < size; i++) {
    CreateGauge(spectrum_data_[i], GetPeak(i), Column(i - half, size - half),
                ftxui::Direction::Down, colorizer, right);
  }

  if (options_.spacing) {
    // Remove last empty space
    left.pop_back();
    right.pop_back();
  }

  visualizer = std::make_shared<EqualHalves>(ftxui::hbox(left) | ftxui::hcenter | ftxui::yflex,
                                             ftxui::hbox(right) | ftxui::hcenter | ftxui::yflex);
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationMono(ftxui::Element& visualizer, const Colorizer& colorizer) {
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
  int total_size = size * (gauge_width_ + (options_.spacing ? kGaugeSpacing : 0));
  entries.reserve(total_size);

  for (int i = 0; i < size; i++) {
    // Peak cap is also the average from both channels
    std::optional<double> peak;
    if (auto peak_left = GetPeak(i), peak_right = GetPeak(i + size); peak_left && peak_right) {
      peak = (*peak_left + *peak_right) / 2;
    }

    CreateGauge(average.at(i), peak, Column(i, size), ftxui::Direction::Up, colorizer, entries);
  }

  if (options_.spacing) {
    // Remove last empty space
    entries.pop_back();
  }

  visualizer = ftxui::hbox(entries) | ftxui::hcenter;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationLine(ftxui::Element& visualizer, LineStyle style,
                                           const Colorizer& colorizer) {
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
  auto draw = [left = std::move(left), right = std::move(right), style,
               colorizer](ftxui::Canvas& canvas) {
    if (canvas.width() < 2 || canvas.height() < 2) {
      return;
    }

    switch (style) {
      case LineStyle::Plain:
        DrawLine(canvas, left, colorizer);
        break;
      case LineStyle::Mirror:
        DrawMirroredLines(canvas, left, right, colorizer);
        break;
      case LineStyle::Filled:
        DrawFilledArea(canvas, left, colorizer);
        break;
      case LineStyle::FilledMirror:
        DrawMirroredFilledAreas(canvas, left, right, colorizer);
        break;
    }

    FillBackground(canvas);
  };

  // Minimum size is a single cell, as canvas fills all the available space
  visualizer = ftxui::canvas(1, 1, std::move(draw)) | ftxui::flex;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationRadial(ftxui::Element& visualizer,
                                             const Colorizer& colorizer) {
  // Data contains both channels (left channel in the first half and right channel in the second)
  const auto size = static_cast<std::ptrdiff_t>(spectrum_data_.size() / 2);
  if (size < 1) return;

  std::vector<double> left(spectrum_data_.begin(), spectrum_data_.begin() + size);
  std::vector<double> right(spectrum_data_.begin() + size, spectrum_data_.begin() + (2 * size));

  auto draw = [left = std::move(left), right = std::move(right),
               colorizer](ftxui::Canvas& canvas) {
    if (canvas.width() < 2 || canvas.height() < 2) return;

    DrawRays(canvas, left, right, colorizer);
    FillBackground(canvas);
  };

  visualizer = ftxui::canvas(1, 1, std::move(draw)) | ftxui::flex;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationWaterfall(ftxui::Element& visualizer,
                                                const Colorizer& colorizer) {
  if (waterfall_.empty()) return;

  // Each cell is colored by its value (instead of the distance from a base, as there is none)
  auto color = [colorizer](double value, double column) {
    return colorizer.At(value, value, column);
  };

  visualizer = std::make_shared<WaterfallRows>(waterfall_, std::move(color));
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawRun(ftxui::Canvas& canvas, const VerticalRun& run,
                                 const Baseline& baseline, const Colorizer& colorizer) {
  const double column = static_cast<double>(run.x) / (canvas.width() - 1);

  for (int y = std::min(run.from, run.to); y <= std::max(run.from, run.to); y++) {
    const double position = static_cast<double>(std::abs(baseline.y - y)) / baseline.range;
    canvas.DrawPoint(run.x, y, true, colorizer.At(position, run.level, column));
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawLine(ftxui::Canvas& canvas, const std::vector<double>& data,
                                  const Colorizer& colorizer) {
  const int bottom = canvas.height() - 1;
  int previous = -1;

  for (int x = 0; x < canvas.width(); x++) {
    const double value = ValueAt(data, x, canvas.width());
    const int y = bottom - static_cast<int>(std::lround(value * bottom));

    // Connect to the previous column, so line keeps continuous even when it is steep
    DrawRun(canvas,
            VerticalRun{.x = x, .from = previous < 0 ? y : previous, .to = y, .level = value},
            Baseline{.y = bottom, .range = bottom}, colorizer);

    previous = y;
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawMirroredLines(ftxui::Canvas& canvas, const std::vector<double>& left,
                                           const std::vector<double>& right,
                                           const Colorizer& colorizer) {
  const int bottom = canvas.height() - 1;
  const int middle = canvas.height() / 2;

  int previous_top = -1;
  int previous_down = -1;

  for (int x = 0; x < canvas.width(); x++) {
    // Left channel goes up from the middle, right channel goes down from it
    const double value_top = ValueAt(left, x, canvas.width());
    const double value_down = ValueAt(right, x, canvas.width());

    const int top = middle - static_cast<int>(std::lround(value_top * middle));
    const int down = middle + static_cast<int>(std::lround(value_down * (bottom - middle)));

    DrawRun(canvas,
            VerticalRun{.x = x,
                        .from = previous_top < 0 ? top : previous_top,
                        .to = top,
                        .level = value_top},
            Baseline{.y = middle, .range = middle}, colorizer);

    DrawRun(canvas,
            VerticalRun{.x = x,
                        .from = previous_down < 0 ? down : previous_down,
                        .to = down,
                        .level = value_down},
            Baseline{.y = middle, .range = bottom - middle}, colorizer);

    previous_top = top;
    previous_down = down;
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawFilledArea(ftxui::Canvas& canvas, const std::vector<double>& data,
                                        const Colorizer& colorizer) {
  const int bottom = canvas.height() - 1;

  for (int x = 0; x < canvas.width(); x++) {
    const double value = ValueAt(data, x, canvas.width());
    const int y = bottom - static_cast<int>(std::lround(value * bottom));

    // Fill everything below the line
    FillBlocks(canvas, VerticalRun{.x = x, .from = y, .to = bottom, .level = value},
               Baseline{.y = bottom, .range = bottom}, colorizer);
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawMirroredFilledAreas(ftxui::Canvas& canvas,
                                                 const std::vector<double>& left,
                                                 const std::vector<double>& right,
                                                 const Colorizer& colorizer) {
  const int bottom = canvas.height() - 1;
  const int middle = canvas.height() / 2;

  for (int x = 0; x < canvas.width(); x++) {
    // Left channel is filled up from the middle, right channel is filled down from it
    const double value_top = ValueAt(left, x, canvas.width());
    const double value_down = ValueAt(right, x, canvas.width());

    const int top = middle - static_cast<int>(std::lround(value_top * middle));
    const int down = middle + static_cast<int>(std::lround(value_down * (bottom - middle)));

    FillBlocks(canvas, VerticalRun{.x = x, .from = top, .to = middle, .level = value_top},
               Baseline{.y = middle, .range = middle}, colorizer);
    FillBlocks(canvas, VerticalRun{.x = x, .from = middle, .to = down, .level = value_down},
               Baseline{.y = middle, .range = bottom - middle}, colorizer);
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::FillBlocks(ftxui::Canvas& canvas, const VerticalRun& run,
                                    const Baseline& baseline, const Colorizer& colorizer) {
  // Blocks are used (instead of braille dots), so the area looks solid. Each block has the height
  // of two braille dots, so it must start from a coordinate multiple of its height
  static constexpr int kBlockHeight = 2;

  const int first = std::min(run.from, run.to);
  const int last = std::max(run.from, run.to);
  const double column = static_cast<double>(run.x) / (canvas.width() - 1);

  for (int block = first - (first % kBlockHeight); block <= last; block += kBlockHeight) {
    const double position = static_cast<double>(std::abs(baseline.y - block)) / baseline.range;
    canvas.DrawBlock(run.x, block, true, colorizer.At(position, run.level, column));
  }
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawRays(ftxui::Canvas& canvas, const std::vector<double>& left,
                                  const std::vector<double>& right, const Colorizer& colorizer) {
  static constexpr double kInnerRadius = 0.35;  //!< Circle where rays start (part of radius)
  static constexpr double kStep = 0.5;          //!< Distance between points from a ray (in dots)
  const double kHalfTurn = std::acos(-1.0);

  // Braille dots are about as wide as they are tall, so the circle uses the smallest dimension
  const double center_x = (canvas.width() - 1) / 2.0;
  const double center_y = (canvas.height() - 1) / 2.0;
  const double radius = std::min(center_x, center_y);
  const double inner = radius * kInnerRadius;
  const double length = radius - inner;

  const auto size = static_cast<int>(left.size());

  // Both channels start from the top, left one going around the left half and right one going
  // around the right half (so lowest frequencies are on top)
  for (int i = 0; i < size; i++) {
    const double angle = (kHalfTurn * (i + 0.5)) / size;

    for (int side = 0; side < 2; side++) {
      const double value = std::clamp(side == 0 ? left[i] : right[i], 0.0, 1.0);
      const double direction = side == 0 ? -1 : 1;

      const double dx = std::sin(angle) * direction;
      const double dy = -std::cos(angle);

      for (double distance = 0; distance <= value * length; distance += kStep) {
        const int x = static_cast<int>(std::lround(center_x + (dx * (inner + distance))));
        const int y = static_cast<int>(std::lround(center_y + (dy * (inner + distance))));
        const double column = radius > 0 ? (x - (center_x - radius)) / (2 * radius) : 0;

        canvas.DrawPoint(x, y, true, colorizer.At(distance / length, value, column));
      }
    }
  }
}

}  // namespace interface
