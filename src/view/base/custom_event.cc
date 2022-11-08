#include "view/base/custom_event.h"

#include <iostream>

namespace interface {

/**
 * @brief Based on Visitor pattern, print content from std::variant used inside CustomEvent
 */
struct CustomEventVisitor {
  explicit CustomEventVisitor(std::ostream& o) : out{o} { out << " content:"; };

  // All mapped types used in the CustomEvent content
  void operator()(int i) const { out << i; }
  void operator()(const model::Song& s) const { out << s; }
  void operator()(const model::Volume& v) const { out << v; }
  void operator()(const model::Song::CurrentInformation& i) const { out << i; }
  void operator()(const std::filesystem::path& p) const { out << p.c_str(); }
  void operator()(const std::vector<double>& v) const { out << "{vector data...}"; }

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

    case CustomEvent::Identifier::Refresh:
      out << "Refresh";
      break;
  }
  return out;
}

//! CustomEvent pretty print
std::ostream& operator<<(std::ostream& out, const CustomEvent& e) {
  out << "{";
  out << " type:" << e.type;
  out << " id:" << e.id;

  std::visit(CustomEventVisitor{out}, e.content);

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
CustomEvent CustomEvent::NotifyFileSelection(const std::filesystem::path file_path) {
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
CustomEvent CustomEvent::Refresh() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::Refresh,
  };
}

}  // namespace interface
