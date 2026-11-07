#include "model/bar_animation.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <string_view>

namespace model {

namespace {

//! Identifier (saved in settings) and name (shown on UI) for each value from an option
struct Naming {
  std::string_view id;
  std::string_view name;
};

constexpr std::array<Naming, static_cast<int>(BarAnimation::LAST - BarAnimation::HorizontalMirror)>
    kAnimations{{
        {"horizontal-mirror", "Horizontal mirror"},
        {"vertical-mirror", "Vertical mirror"},
        {"mono", "Mono"},
        {"line", "Line"},
        {"line-mirror", "Line (mirror)"},
        {"line-filled", "Line (filled)"},
        {"line-filled-mirror", "Line (filled mirror)"},
        {"radial", "Radial"},
        {"waterfall", "Waterfall"},
    }};

constexpr std::array<Naming, static_cast<int>(BarStyle::LAST)> kBarStyles{{
    {"solid", "Solid"},
    {"led", "LED blocks"},
    {"braille", "Braille"},
}};

constexpr std::array<Naming, static_cast<int>(BarColors::LAST)> kBarColors{{
    {"vertical", "Vertical"},
    {"level", "By level"},
    {"level-stretched", "By level (stretched)"},
    {"horizontal", "Horizontal"},
    {"diagonal", "Diagonal"},
    {"zones", "Zones"},
}};

constexpr std::array<Naming, static_cast<int>(HueCycle::LAST)> kHueCycles{{
    {"off", "Off"},
    {"on", "On"},
    {"flash", "On + beat flash"},
}};

//! Get naming for the given value (or nothing when it is out of range)
template <typename T, std::size_t N>
const Naming* Find(const std::array<Naming, N>& namings, T value, T first) {
  const auto index = static_cast<int>(value) - static_cast<int>(first);
  if (index < 0 || index >= static_cast<int>(N)) return nullptr;
  return &namings[static_cast<std::size_t>(index)];
}

//! Get value from the given identifier (or nothing when it is unknown)
template <typename T, std::size_t N>
std::optional<T> FromId(const std::array<Naming, N>& namings, std::string_view id, T first) {
  const auto found = std::find_if(namings.begin(), namings.end(),
                                  [id](const Naming& naming) { return naming.id == id; });
  if (found == namings.end()) return std::nullopt;

  return static_cast<T>(static_cast<int>(first) + static_cast<int>(found - namings.begin()));
}

}  // namespace

/* ********************************************************************************************** */

//! BarAnimation pretty print
std::ostream& operator<<(std::ostream& out, const BarAnimation& animation) {
  static constexpr std::array<std::string_view, kAnimations.size()> kSymbols{
      "HorizontalMirror",   "VerticalMirror",           "Mono",
      "SpectrumLine",       "SpectrumLineMirror",       "SpectrumLineFilled",
      "SpectrumLineFilledMirror", "Radial",             "Waterfall",
  };

  const auto index = static_cast<int>(animation - BarAnimation::HorizontalMirror);
  const bool valid = index >= 0 && index < static_cast<int>(kSymbols.size());

  out << std::quoted(valid ? kSymbols[static_cast<std::size_t>(index)] : "Invalid");
  return out;
}

/* ********************************************************************************************** */

std::string_view GetAnimationName(const BarAnimation& animation) {
  const auto* naming = Find(kAnimations, animation, BarAnimation::HorizontalMirror);
  return naming ? naming->name : "Invalid";
}

std::string_view GetAnimationId(const BarAnimation& animation) {
  const auto* naming = Find(kAnimations, animation, BarAnimation::HorizontalMirror);
  return naming ? naming->id : "invalid";
}

std::optional<BarAnimation> GetAnimationFromId(std::string_view id) {
  return FromId(kAnimations, id, BarAnimation::HorizontalMirror);
}

/* ********************************************************************************************** */

std::string_view GetName(BarStyle style) {
  const auto* naming = Find(kBarStyles, style, BarStyle::Solid);
  return naming ? naming->name : "Invalid";
}

std::string_view GetName(BarColors colors) {
  const auto* naming = Find(kBarColors, colors, BarColors::Vertical);
  return naming ? naming->name : "Invalid";
}

std::string_view GetName(HueCycle hue) {
  const auto* naming = Find(kHueCycles, hue, HueCycle::Off);
  return naming ? naming->name : "Invalid";
}

/* ********************************************************************************************** */

std::string_view GetId(BarStyle style) {
  const auto* naming = Find(kBarStyles, style, BarStyle::Solid);
  return naming ? naming->id : "invalid";
}

std::string_view GetId(BarColors colors) {
  const auto* naming = Find(kBarColors, colors, BarColors::Vertical);
  return naming ? naming->id : "invalid";
}

std::string_view GetId(HueCycle hue) {
  const auto* naming = Find(kHueCycles, hue, HueCycle::Off);
  return naming ? naming->id : "invalid";
}

/* ********************************************************************************************** */

std::optional<BarStyle> GetBarStyleFromId(std::string_view id) {
  return FromId(kBarStyles, id, BarStyle::Solid);
}

std::optional<BarColors> GetBarColorsFromId(std::string_view id) {
  return FromId(kBarColors, id, BarColors::Vertical);
}

std::optional<HueCycle> GetHueCycleFromId(std::string_view id) {
  return FromId(kHueCycles, id, HueCycle::Off);
}

}  // namespace model
