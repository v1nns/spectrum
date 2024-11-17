#include "view/base/custom_event.h"

#include <iomanip>
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
  void operator()(const std::filesystem::path& p) const { out << std::quoted(p.c_str()); }
  void operator()(const std::vector<double>&) const { out << "{vector data...}"; }
  void operator()(const model::EqualizerPreset&) const {
    // TODO: maybe implement detailed info here
    out << "{audio filter data...}";
  }
  void operator()(const model::BarAnimation& a) const { out << a; }
  void operator()(const model::BlockIdentifier& i) const { out << i; }
  void operator()(const model::Playlist& p) const { out << p; }
  void operator()(const model::PlaylistOperation& p) const { out << p; }

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

    case CustomEvent::Identifier::NotifyPlaylistSelection:
      out << "NotifyPlaylistSelection";
      break;

    case CustomEvent::Identifier::Refresh:
      out << "Refresh";
      break;

    case CustomEvent::Identifier::EnableGlobalEvent:
      out << "EnableGlobalEvent";
      break;

    case CustomEvent::Identifier::DisableGlobalEvent:
      out << "DisableGlobalEvent";
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

    case CustomEvent::Identifier::UpdateBarWidth:
      out << "UpdateBarWidth";
      break;

    case CustomEvent::Identifier::SkipToNextSong:
      out << "SkipToNextSong";
      break;

    case CustomEvent::Identifier::SkipToPreviousSong:
      out << "SkipToPreviousSong";
      break;

    case CustomEvent::Identifier::ShowPlaylistManager:
      out << "ShowPlaylistManager";
      break;

    case CustomEvent::Identifier::SavePlaylistsToFile:
      out << "SavePlaylistsToFile";
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

CustomEvent CustomEvent::ClearSongInfo() {
  return CustomEvent{
      .type = Type::FromAudioThreadToInterface,
      .id = Identifier::ClearSongInfo,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::UpdateVolume(const model::Volume& sound_volume) {
  return CustomEvent{
      .type = Type::FromAudioThreadToInterface,
      .id = Identifier::UpdateVolume,
      .content = sound_volume,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::UpdateSongInfo(const model::Song& info) {
  return CustomEvent{
      .type = Type::FromAudioThreadToInterface,
      .id = Identifier::UpdateSongInfo,
      .content = info,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::UpdateSongState(const model::Song::CurrentInformation& new_state) {
  return CustomEvent{
      .type = Type::FromAudioThreadToInterface,
      .id = Identifier::UpdateSongState,
      .content = new_state,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::DrawAudioSpectrum(const std::vector<double>& data) {
  return CustomEvent{
      .type = Type::FromAudioThreadToInterface,
      .id = Identifier::DrawAudioSpectrum,
      .content = data,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::NotifyFileSelection(const std::filesystem::path& file_path) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::NotifyFileSelection,
      .content = file_path,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::PauseOrResumeSong() {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::PauseOrResumeSong,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::StopSong() {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::StopSong,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::SetAudioVolume(const model::Volume& sound_volume) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::SetAudioVolume,
      .content = sound_volume,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::ResizeAnalysis(int bars) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::ResizeAnalysis,
      .content = bars,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::SeekForwardPosition(int offset) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::SeekForwardPosition,
      .content = offset,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::SeekBackwardPosition(int offset) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::SeekBackwardPosition,
      .content = offset,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::ApplyAudioFilters(const model::EqualizerPreset& filters) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::ApplyAudioFilters,
      .content = filters,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::NotifyPlaylistSelection(const model::Playlist& playlist) {
  return CustomEvent{
      .type = Type::FromInterfaceToAudioThread,
      .id = Identifier::NotifyPlaylistSelection,
      .content = playlist,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::Refresh() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::Refresh,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::EnableGlobalEvent() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::EnableGlobalEvent,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::DisableGlobalEvent() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::DisableGlobalEvent,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::ChangeBarAnimation(const model::BarAnimation& animation) {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::ChangeBarAnimation,
      .content = animation,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::ShowHelper() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::ShowHelper,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::CalculateNumberOfBars(int number) {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::CalculateNumberOfBars,
      .content = number,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::SetPreviousFocused() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::SetPreviousFocused,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::SetNextFocused() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::SetNextFocused,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::SetFocused(const model::BlockIdentifier& id) {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::SetFocused,
      .content = id,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::PlaySong() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::PlaySong,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::ToggleFullscreen() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::ToggleFullscreen,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::UpdateBarWidth() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::UpdateBarWidth,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::SkipToNextSong() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::SkipToNextSong,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::SkipToPreviousSong() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::SkipToPreviousSong,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::ShowPlaylistManager(const model::PlaylistOperation& operation) {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::ShowPlaylistManager,
      .content = operation,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::SavePlaylistsToFile(const model::Playlist& changed_playlist) {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::SavePlaylistsToFile,
      .content = changed_playlist,
  };
}

/* ********************************************************************************************** */

CustomEvent CustomEvent::Exit() {
  return CustomEvent{
      .type = Type::FromInterfaceToInterface,
      .id = Identifier::Exit,
  };
}

}  // namespace interface
