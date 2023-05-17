#include "view/base/custom_event.h"

#include <iostream>

namespace interface {

/**
 * @brief Based on Visitor pattern, print content from std::variant used inside CustomEvent
 */
struct ContentVisitor {
  explicit ContentVisitor(std::ostream& o) : out{o} { out << " content:"; };

  // All mapped types used in the CustomEvent content
  void operator()(const std::monostate&) const { out << "empty"; }
  void operator()(int i) const { out << i; }
  void operator()(const model::Song& s) const { out << s; }
  void operator()(const model::Volume& v) const { out << v; }
  void operator()(const model::Song::CurrentInformation& i) const { out << i; }
  void operator()(const std::filesystem::path& p) const { out << p.c_str(); }
  void operator()(const std::vector<double>&) const { out << "{vector data...}"; }
  void operator()(const model::EqualizerPreset&) const {
    // TODO: maybe implement detailed info here
    out << "{audio filter data...}";
  }
  void operator()(const model::BarAnimation& a) const { out << a; }
  void operator()(const model::BlockIdentifier& i) const { out << i; }

  std::ostream& out;
};

//! CustomEvent::Type pretty print
std::ostream& operator<<(std::ostream& out, const CustomEvent::Type& t) {
  switch (t) {
    case CustomEvent::Type::FromInterfaceToAudioThread:
      out << "{UI->Player}";
      break;

    case CustomEvent::Type::FromAudioThreadToInterface:
      out << "{Player->UI}";
      break;

    case CustomEvent::Type::FromInterfaceToInterface:
      out << "{UI->UI}";
      break;
  }
  return out;
}

//! CustomEvent::Identifier pretty print
std::ostream& operator<<(std::ostream& out, const CustomEvent::Identifier& i) {
  switch (i) {
    case CustomEvent::Identifier::ClearSongInfo:
      out << "ClearSongInfo";
      break;

    case CustomEvent::Identifier::UpdateVolume:
      out << "UpdateVolume";
      break;

    case CustomEvent::Identifier::UpdateSongInfo:
      out << "UpdateSongInfo";
      break;

    case CustomEvent::Identifier::UpdateSongState:
      out << "UpdateSongState";
      break;

    case CustomEvent::Identifier::DrawAudioSpectrum:
      out << "DrawAudioSpectrum";
      break;

    case CustomEvent::Identifier::NotifyFileSelection:
      out << "NotifyFileSelection";
      break;

    case CustomEvent::Identifier::PauseOrResumeSong:
      out << "PauseOrResumeSong";
      break;

    case CustomEvent::Identifier::StopSong:
      out << "StopSong";
      break;

    case CustomEvent::Identifier::ClearCurrentSong:
      out << "ClearCurrentSong";
      break;

    case CustomEvent::Identifier::SetAudioVolume:
      out << "SetAudioVolume";
      break;

    case CustomEvent::Identifier::ResizeAnalysis:
      out << "ResizeAnalysis";
      break;

    case CustomEvent::Identifier::SeekForwardPosition:
      out << "SeekForwardPosition";
      break;

    case CustomEvent::Identifier::SeekBackwardPosition:
      out << "SeekBackwardPosition";
      break;

    case CustomEvent::Identifier::ApplyAudioFilters:
      out << "ApplyAudioFilters";
      break;

    case CustomEvent::Identifier::Refresh:
      out << "Refresh";
      break;

    case CustomEvent::Identifier::ChangeBarAnimation:
      out << "ChangeBarAnimation";
      break;

    case CustomEvent::Identifier::ShowHelper:
      out << "ShowHelper";
      break;

    case CustomEvent::Identifier::CalculateNumberOfBars:
      out << "CalculateNumberOfBars";
      break;

    case CustomEvent::Identifier::SetPreviousFocused:
      out << "SetPreviousFocused";
      break;

    case CustomEvent::Identifier::SetNextFocused:
      out << "SetNextFocused";
      break;

    case CustomEvent::Identifier::SetFocused:
      out << "SetFocused";
      break;

    case CustomEvent::Identifier::PlaySong:
      out << "PlaySong";
      break;

    case CustomEvent::Identifier::ToggleFullscreen:
      out << "ToggleFullscreen";
      break;

    case CustomEvent::Identifier::Exit:
      out << "Exit";
      break;
  }
  return out;
}

//! CustomEvent pretty print
std::ostream& operator<<(std::ostream& out, const CustomEvent& e) {
  out << "{";
  out << " type:" << e.type;
  out << " id:" << e.id;

  std::visit(ContentVisitor{out}, e.content);

  out << " }";

  return out;
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::ClearSongInfo() {
  return CustomEvent{
      .type = Type::FromAudioThreadToInterface,
      .id = Identifier::ClearSongInfo,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::UpdateVolume(const model::Volume& sound_volume) {
  return CustomEvent{
      .type = Type::FromAudioThreadToInterface,
      .id = Identifier::UpdateVolume,
      .content = sound_volume,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::UpdateSongInfo(const model::Song& info) {
  return CustomEvent{
      .type = Type::FromAudioThreadToInterface,
      .id = Identifier::UpdateSongInfo,
      .content = info,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::UpdateSongState(const model::Song::CurrentInformation& new_state) {
  return CustomEvent{
      .type = Type::FromAudioThreadToInterface,
      .id = Identifier::UpdateSongState,
      .content = new_state,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::DrawAudioSpectrum(const std::vector<double>& data) {
  return CustomEvent{
      .type = Type::FromAudioThreadToInterface,
      .id = Identifier::DrawAudioSpectrum,
      .content = data,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::NotifyFileSelection(const std::filesystem::path& file_path) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::NotifyFileSelection,
      .content = file_path,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::PauseOrResumeSong() {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::PauseOrResumeSong,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::StopSong() {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::StopSong,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::ClearCurrentSong() {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::ClearCurrentSong,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::SetAudioVolume(const model::Volume& sound_volume) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::SetAudioVolume,
      .content = sound_volume,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::ResizeAnalysis(int bars) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::ResizeAnalysis,
      .content = bars,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::SeekForwardPosition(int offset) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::SeekForwardPosition,
      .content = offset,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::SeekBackwardPosition(int offset) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::SeekBackwardPosition,
      .content = offset,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::ApplyAudioFilters(const model::EqualizerPreset& filters) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::ApplyAudioFilters,
      .content = filters,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::Refresh() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::Refresh,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::ChangeBarAnimation(const model::BarAnimation& animation) {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::ChangeBarAnimation,
      .content = animation,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::ShowHelper() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::ShowHelper,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::CalculateNumberOfBars(int number) {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::CalculateNumberOfBars,
      .content = number,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::SetPreviousFocused() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::SetPreviousFocused,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::SetNextFocused() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::SetNextFocused,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::SetFocused(const model::BlockIdentifier& id) {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::SetFocused,
      .content = id,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::PlaySong() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::PlaySong,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::ToggleFullscreen() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::ToggleFullscreen,
  };
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::Exit() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::Exit,
  };
}

}  // namespace interface
