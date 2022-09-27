#include "middleware/media_controller.h"

#include <string>
#include <thread>

#include "audio/player.h"
#include "ftxui/component/event.hpp"
#include "model/application_error.h"
#include "model/song.h"
#include "view/base/block.h"
#include "view/base/terminal.h"

namespace middleware {

std::shared_ptr<MediaController> MediaController::Create(
    const std::shared_ptr<interface::Terminal>& terminal,
    const std::shared_ptr<audio::Player>& player, bool asynchronous) {
  // Create and initialize media controller
  auto controller = std::shared_ptr<MediaController>(new MediaController(terminal, player));
  controller->Init(asynchronous);

  // Register callbacks to Terminal
  terminal->RegisterInterfaceListener(controller);

  // Register callbacks to Player
  player->RegisterInterfaceNotifier(controller);

  // TODO: Think of a better way to do this...
  model::Volume value = player->GetAudioVolume();
  auto event = interface::CustomEvent::UpdateVolume(value);
  terminal->QueueEvent(event);

  return controller;
}

/* ********************************************************************************************** */

MediaController::MediaController(const std::shared_ptr<interface::EventDispatcher>& dispatcher,
                                 const std::shared_ptr<audio::AudioControl>& player_ctl)
    : interface::Listener(),
      interface::Notifier(),
      dispatcher_{dispatcher},
      player_ctl_{player_ctl},
      analyzer_{},
      analysis_loop_{},
      analysis_data_{.exit = false} {}

/* ********************************************************************************************** */

MediaController::~MediaController() {
  Exit();

  if (analysis_loop_.joinable()) {
    analysis_loop_.join();
  }
}

/* ********************************************************************************************** */

void MediaController::Init(bool asynchronous) {
  analyzer_ = std::make_unique<driver::FFTW>();

  // Initialize internal structures
  analyzer_->Init();

  if (asynchronous) {
    // Spawn thread for Audio Analysis
    analysis_loop_ = std::thread(&MediaController::AnalysisHandler, this);
  }
}

/* ********************************************************************************************** */

void MediaController::Exit() {
  analysis_data_.exit = true;
  analysis_data_.notifier.notify_one();
}

/* ********************************************************************************************** */

void MediaController::AnalysisHandler() {
  // Get buffer size directly from audio analyzer, to discover chunk size to send and receive
  int input_size = analyzer_->GetBufferSize();
  int output_size = analyzer_->GetOutputSize();

  std::vector<double> result(output_size, 0);

  while (analysis_data_.WaitForInput()) {
    std::vector<double> input = analysis_data_.GetData(input_size);
    analyzer_->Execute(input.data(), input_size, result.data());

    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) continue;

    auto event = interface::CustomEvent::DrawAudioSpectrum(result);
    dispatcher->SendEvent(event);
  }
}

/* ********************************************************************************************** */

void MediaController::NotifyFileSelection(const std::filesystem::path& filepath) {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Play(filepath);
}

/* ********************************************************************************************** */

void MediaController::ClearCurrentSong() {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Stop();
}

/* ********************************************************************************************** */

void MediaController::PauseOrResume() {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->PauseOrResume();
}

/* ********************************************************************************************** */

void MediaController::SetVolume(model::Volume value) {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->SetAudioVolume(value);
}

/* ********************************************************************************************** */

void MediaController::ClearSongInformation() {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::ClearSongInfo();

  // Notify File Info block with to clear info about song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::NotifySongInformation(const model::Song& info) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::UpdateSongInfo(info);

  // Notify File Info block with information about the recently loaded song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::NotifySongState(const model::Song::CurrentInformation& state) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::UpdateSongState(state);

  // Notify Audio Player block with new state information about the current song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::SendAudioRaw(int* buffer, int buffer_size) {
  std::unique_lock<std::mutex> lock(analysis_data_.mutex);

  // Append input data to internal buffer
  std::vector<double>::const_iterator end = analysis_data_.buffer.end();
  analysis_data_.buffer.insert(end, buffer, buffer + buffer_size);
  analysis_data_.notifier.notify_one();
}

/* ********************************************************************************************** */

void MediaController::NotifyError(error::Code code) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  // Notify Terminal about error that has occurred in Audio thread
  dispatcher->SetApplicationError(code);
}

}  // namespace middleware
