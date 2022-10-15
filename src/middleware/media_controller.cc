#include "middleware/media_controller.h"

#include <string>
#include <thread>

#include "audio/driver/fftw.h"
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
  // Instantiate FFTW to run audio analysis
  std::unique_ptr<driver::Analyzer> analyzer(new driver::FFTW);

  // Create and initialize media controller
  auto controller =
      std::shared_ptr<MediaController>(new MediaController(terminal, player, std::move(analyzer)));

  // Use terminal maximum width as input to decide how many bars should display on audio visualizer
  auto number_bars = terminal->CalculateNumberBars();
  controller->Init(number_bars, asynchronous);

  // Register callbacks to Terminal
  terminal->RegisterInterfaceListener(controller);

  // Register callbacks to Player
  player->RegisterInterfaceNotifier(controller);

  // TODO: Think of a better way to do this...
  model::Volume value = player->GetAudioVolume();
  auto event_vol = interface::CustomEvent::UpdateVolume(value);
  terminal->ProcessEvent(event_vol);

  auto event_bars =
      interface::CustomEvent::DrawAudioSpectrum(std::vector<double>(number_bars, 0.001));
  terminal->ProcessEvent(event_bars);

  return controller;
}

/* ********************************************************************************************** */

MediaController::MediaController(const std::shared_ptr<interface::EventDispatcher>& dispatcher,
                                 const std::shared_ptr<audio::AudioControl>& player_ctl,
                                 std::unique_ptr<driver::Analyzer>&& analyzer)
    : interface::Listener(),
      interface::Notifier(),
      dispatcher_{dispatcher},
      player_ctl_{player_ctl},
      analyzer_{std::move(analyzer)},
      analysis_loop_{},
      analysis_data_{} {}

/* ********************************************************************************************** */

MediaController::~MediaController() {
  Exit();

  if (analysis_loop_.joinable()) {
    analysis_loop_.join();
  }
}

/* ********************************************************************************************** */

void MediaController::Init(int number_bars, bool asynchronous) {
  // Initialize internal structures
  analyzer_->Init(number_bars);

  if (asynchronous) {
    // Spawn thread for Audio Analysis
    analysis_loop_ = std::thread(&MediaController::AnalysisHandler, this);
  }
}

/* ********************************************************************************************** */

void MediaController::Exit() { analysis_data_.Push(Command::Exit); }

/* ********************************************************************************************** */

void MediaController::AnalysisHandler() {
  using namespace std::chrono_literals;
  std::vector<double> input, output, previous;
  int in_size, out_size;

  while (analysis_data_.WaitForCommand()) {
    // Get buffer size directly from audio analyzer, to discover chunk size to receive and send
    in_size = analyzer_->GetBufferSize();
    out_size = analyzer_->GetOutputSize();

    // Resize output vector if necessary
    if (output.size() != out_size) {
      output.resize(out_size);
    }

    auto command = analysis_data_.Pop();

    switch (command) {
      case Command::Analyze: {
        // Get input data and run FFT
        input = analysis_data_.GetBuffer(in_size);
        analyzer_->Execute(input.data(), input.size(), output.data());

        auto dispatcher = dispatcher_.lock();
        if (!dispatcher) continue;

        // Send result to UI
        auto event = interface::CustomEvent::DrawAudioSpectrum(output);
        dispatcher->SendEvent(event);

        previous = output;
      } break;

      case Command::RunClearAnimation: {
        auto dispatcher = dispatcher_.lock();
        if (!dispatcher) continue;

        for (int i = 0; i < 10; i++) {
          std::transform(previous.begin(), previous.end(), previous.begin(),
                         std::bind(std::multiplies<double>(), std::placeholders::_1, 0.45));

          // Send result to UI
          auto event = interface::CustomEvent::DrawAudioSpectrum(previous);
          dispatcher->SendEvent(event);

          std::this_thread::sleep_for(0.04s);
        }

        previous = std::vector(previous.size(), 0.001);

        auto event = interface::CustomEvent::DrawAudioSpectrum(previous);
        dispatcher->SendEvent(event);

        analysis_data_.Push(Command::RunRegainAnimation);
      } break;

      case Command::RunRegainAnimation: {
        auto dispatcher = dispatcher_.lock();
        if (!dispatcher) continue;

        std::vector<double> bar_values;

        for (int i = 1; i <= 10; i++) {
          bar_values.clear();

          for (auto& value : output) {
            bar_values.push_back((value / 10) * i);
          }

          // Send result to UI
          auto event = interface::CustomEvent::DrawAudioSpectrum(bar_values);
          dispatcher->SendEvent(event);

          std::this_thread::sleep_for(0.01s);
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

void MediaController::ResizeAnalysisOutput(int value) {
  std::unique_lock<std::mutex> lock(analysis_data_.mutex);
  analyzer_->Init(value);
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
  if (state.state == model::Song::MediaState::Pause)
    analysis_data_.Push(Command::RunClearAnimation);

  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::UpdateSongState(state);

  // Notify Audio Player block with new state information about the current song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::SendAudioRaw(int* buffer, int buffer_size) {
  // std::ofstream myfile;
  // myfile.open("/tmp/output.txt", std::ios::out | std::ios::app | std::ios::binary);
  // myfile << "ta adicionando isso no buffer: " << buffer_size << "\n";
  // myfile.close();

  analysis_data_.Append(buffer, buffer_size);
}

/* ********************************************************************************************** */

void MediaController::NotifyError(error::Code code) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  // Notify Terminal about error that has occurred in Audio thread
  dispatcher->SetApplicationError(code);
}

}  // namespace middleware
