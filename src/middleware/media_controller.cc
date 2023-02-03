#include "middleware/media_controller.h"

#include <string>
#include <thread>

#include "audio/driver/fftw.h"
#include "audio/player.h"
#include "ftxui/component/event.hpp"
#include "model/application_error.h"
#include "model/song.h"
#include "util/logger.h"
#include "view/base/block.h"
#include "view/base/terminal.h"

namespace middleware {

std::shared_ptr<MediaController> MediaController::Create(
    const std::shared_ptr<interface::EventDispatcher>& terminal,
    const std::shared_ptr<audio::AudioControl>& player, driver::Analyzer* analyzer,
    bool asynchronous) {
  LOG("Create new instance of media controller");

  // Instantiate FFTW to run audio analysis
  auto an = analyzer != nullptr ? std::unique_ptr<driver::Analyzer>(std::move(analyzer))
                                : std::make_unique<driver::FFTW>();

  // Create and initialize media controller
  auto controller =
      std::shared_ptr<MediaController>(new MediaController(terminal, player, std::move(an)));

  // TODO: remove this from here, move to main?
  // Use terminal maximum width as input to decide how many bars should display on audio visualizer
  auto number_bars = terminal->CalculateNumberBars();
  controller->Init(number_bars, asynchronous);

  auto event_bars =
      interface::CustomEvent::DrawAudioSpectrum(std::vector<double>(number_bars, 0.001));
  terminal->ProcessEvent(event_bars);

  return controller;
}

/* ********************************************************************************************** */

MediaController::MediaController(const std::shared_ptr<interface::EventDispatcher>& dispatcher,
                                 const std::shared_ptr<audio::AudioControl>& player_ctl,
                                 std::unique_ptr<driver::Analyzer>&& analyzer)
    : audio::Notifier(),
      interface::Notifier(),
      dispatcher_{dispatcher},
      player_ctl_{player_ctl},
      analyzer_{std::move(analyzer)},
      analysis_loop_{},
      sync_data_{} {}

/* ********************************************************************************************** */

MediaController::~MediaController() {
  Exit();

  if (analysis_loop_.joinable()) {
    analysis_loop_.join();
  }
}

/* ********************************************************************************************** */

void MediaController::Init(int number_bars, bool asynchronous) {
  LOG("Initialize media controller with number_bars=", number_bars, " and async=", asynchronous);

  // Initialize internal structures
  analyzer_->Init(number_bars);

  if (asynchronous) {
    // Spawn thread for Audio Analysis
    analysis_loop_ = std::thread(&MediaController::AnalysisHandler, this);
  }
}

/* ********************************************************************************************** */

void MediaController::Exit() {
  LOG("Add command to queue: Exit");
  sync_data_.Push(Command::Exit);
}

/* ********************************************************************************************** */

void MediaController::AnalysisHandler() {
  LOG("Start analysis handler thread");

  using namespace std::chrono_literals;
  std::vector<double> input, output, previous;
  int in_size, out_size;

  while (sync_data_.WaitForCommand()) {
    // Get buffer size directly from audio analyzer, to discover chunk size to receive and send
    in_size = analyzer_->GetBufferSize();
    out_size = analyzer_->GetOutputSize();

    // Resize output vector if necessary
    if (output.size() != out_size) {
      output.resize(out_size);
    }

    auto command = sync_data_.Pop();

    switch (command) {
      case Command::Analyze: {
        // Get input data, run FFT and update local cache
        // P.S.: do not log this because this command is received too often
        input = sync_data_.GetBuffer(in_size);
        analyzer_->Execute(input.data(), input.size(), output.data());
        previous = output;

        auto dispatcher = dispatcher_.lock();
        if (!dispatcher) continue;

        // Send result to UI
        auto event = interface::CustomEvent::DrawAudioSpectrum(output);
        dispatcher->SendEvent(event);

      } break;

      case Command::RunClearAnimationWithRegain:
      case Command::RunClearAnimationWithoutRegain: {
        LOG("Analysis handler received command to run clear animation on audio visualizer");
        auto dispatcher = dispatcher_.lock();
        if (!dispatcher) continue;

        for (int i = 0; i < 10; i++) {
          // Each time this loop is executed, it will reduce spectrum bar values to 45% based on its
          // previous values (this value was decided based on feeling :P)
          std::transform(previous.begin(), previous.end(), previous.begin(),
                         std::bind(std::multiplies<double>(), std::placeholders::_1, 0.45));

          // Send result to UI
          auto event = interface::CustomEvent::DrawAudioSpectrum(previous);
          dispatcher->SendEvent(event);

          // Sleep a little bit before sending a new update to UI. And in case of receiving a new
          // command in the meantime, just cancel animation
          auto timeout = std::chrono::system_clock::now() + 0.04s;
          bool exit_animation = sync_data_.WaitForCommandOrUntil(timeout);
          if (exit_animation) break;
        }

        previous = std::vector(previous.size(), 0.001);

        auto event = interface::CustomEvent::DrawAudioSpectrum(previous);
        dispatcher->SendEvent(event);

        // Enqueue to run regain animation when song is resumed
        if (command == Command::RunClearAnimationWithRegain)
          sync_data_.Push(Command::RunRegainAnimation);

      } break;

      case Command::RunRegainAnimation: {
        LOG("Analysis handler received command to run regain animation on audio visualizer");
        auto dispatcher = dispatcher_.lock();
        if (!dispatcher) continue;

        std::vector<double> bars;

        for (int i = 1; i <= 10; i++) {
          // Each time this loop is executed, it will increase spectrum bar values in a step of 10%
          // based on its previous values (this value was also decided based on feeling)
          for (auto& value : output) bars.push_back((value / 10) * i);

          // Send result to UI
          auto event = interface::CustomEvent::DrawAudioSpectrum(bars);
          dispatcher->SendEvent(event);

          // Sleep a little bit before sending a new update to UI. And in case of receiving a new
          // command in the meantime, just cancel animation
          auto timeout = std::chrono::system_clock::now() + 0.01s;
          bool exit_animation = sync_data_.WaitForCommandOrUntil(timeout);
          if (exit_animation) break;

          bars.clear();
        }
      } break;

      default:
        break;
    }
  }
}

/* ********************************************************************************************** */

void MediaController::NotifyFileSelection(const std::filesystem::path& filepath) {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Play(filepath);
}

/* ********************************************************************************************** */

void MediaController::PauseOrResume() {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->PauseOrResume();
}

/* ********************************************************************************************** */

void MediaController::Stop() {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Stop();
}

/* ********************************************************************************************** */

void MediaController::ClearCurrentSong() {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Stop();
}

/* ********************************************************************************************** */

void MediaController::SetVolume(model::Volume value) {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->SetAudioVolume(value);
}

/* ********************************************************************************************** */

void MediaController::ResizeAnalysisOutput(int value) {
  std::unique_lock<std::mutex> lock(sync_data_.mutex);
  analyzer_->Init(value);
}

/* ********************************************************************************************** */

void MediaController::SeekForwardPosition(int value) {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->SeekForwardPosition(value);
}

/* ********************************************************************************************** */

void MediaController::SeekBackwardPosition(int value) {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->SeekBackwardPosition(value);
}

/* ********************************************************************************************** */

void MediaController::ApplyAudioFilters(const std::vector<model::AudioFilter>& filters) {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->ApplyAudioFilters(filters);
}

/* ********************************************************************************************** */

void MediaController::ClearSongInformation(bool playing) {
  if (playing) sync_data_.Push(Command::RunClearAnimationWithoutRegain);

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
  // Enqueue animation to thread
  if (state.state == model::Song::MediaState::Pause) {
    sync_data_.Push(Command::RunClearAnimationWithRegain);
  } else if (state.state == model::Song::MediaState::Stop) {
    sync_data_.Push(Command::RunClearAnimationWithoutRegain);
  }

  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::UpdateSongState(state);

  // Notify Audio Player block with new state information about the current song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::SendAudioRaw(uint8_t* buffer, int buffer_size) {
  // Append audio data to be analyzed by thread
  sync_data_.Append(buffer, buffer_size);
}

/* ********************************************************************************************** */

void MediaController::NotifyError(error::Code code) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  // Notify Terminal about error that has occurred in Audio thread
  dispatcher->SetApplicationError(code);
}

}  // namespace middleware
