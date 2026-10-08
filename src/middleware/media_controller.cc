#include "middleware/media_controller.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>

#ifndef SPECTRUM_DEBUG
#include "audio/driver/fftw.h"
#else
#include "debug/dummy_analyzer.h"
#endif

#include "audio/player.h"
#include "model/application_error.h"
#include "model/song.h"
#include "util/logger.h"
#include "view/base/block.h"
#include "view/base/terminal.h"

namespace middleware {

std::shared_ptr<MediaController> MediaController::Create(
    const std::shared_ptr<interface::EventDispatcher>& terminal,
    const std::shared_ptr<audio::AudioControl>& player, int number_bars, driver::Analyzer* analyzer,
    bool asynchronous) {
  LOG("Create new instance of media controller");

#ifndef SPECTRUM_DEBUG
  // Instantiate FFTW to run audio analysis
  auto an = analyzer != nullptr ? std::unique_ptr<driver::Analyzer>(std::move(analyzer))
                                : std::make_unique<driver::FFTW>();
#else
  // Create analyzer object
  auto an = std::make_unique<driver::DummyAnalyzer>();
#endif

  // Simply extend the MediaController class, as we do not want to expose the default constructor,
  // neither do we want to use std::make_shared explicitly calling operator new()
  struct MakeSharedEnabler : public MediaController {
    explicit MakeSharedEnabler(const std::shared_ptr<interface::EventDispatcher>& dispatcher,
                               const std::shared_ptr<audio::AudioControl>& player_ctl,
                               std::unique_ptr<driver::Analyzer>&& analyzer)
        : MediaController(dispatcher, player_ctl, std::move(analyzer)) {}
  };

  // Create and initialize media controller
  auto controller = std::make_shared<MakeSharedEnabler>(terminal, player, std::move(an));

  controller->Init(number_bars, asynchronous);

  // As we have no audio analysis output at this point, simply create a dummy output to show in UI
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
      analyzer_{std::move(analyzer)} {}

/* ********************************************************************************************** */

MediaController::~MediaController() {
  try {
    Exit();
  } catch (...) {
    // We don't mind about exceptions at this point in life
  }

  if (analysis_loop_.joinable()) {
    analysis_loop_.join();
  }
}

/* ********************************************************************************************** */

void MediaController::Init(int number_bars, bool asynchronous) {
  LOG("Initialize media controller with number_bars=", number_bars, " and async=", asynchronous);
  finished_ = false;

  // Initialize internal structures
  analyzer_->Init(number_bars);

  if (asynchronous) {
    // Spawn thread for Audio Analysis
    analysis_loop_ = std::thread(&MediaController::AnalysisHandler, this);
  }
}

/* ********************************************************************************************** */

void MediaController::Exit() {
  if (finished_) {
    // Media controller already exited from loop
    return;
  }

  LOG("Add command to queue: \"Exit\"");
  sync_data_.Push(Command::Exit);
}

/* ********************************************************************************************** */

model::PlayerStatus MediaController::GetStatus() const {
  std::scoped_lock lock(status_mutex_);
  return status_;
}

/* ********************************************************************************************** */

void MediaController::SetStatusListener(StatusListener listener) {
  std::scoped_lock lock(status_mutex_);
  status_listener_ = std::move(listener);

  if (status_listener_) status_listener_(status_);
}

/* ********************************************************************************************** */

void MediaController::AnalysisHandler() {
  util::Logger::SetThreadName("analysis");
  LOG("Start analysis handler thread");

  std::vector<double> input, output, previous;

  // Start time of fade-in animation (only set while animation is running)
  std::optional<std::chrono::steady_clock::time_point> fade_in_start;

  while (sync_data_.WaitForCommand()) {
    // Get buffer size directly from audio analyzer, to discover chunk size to receive and send
    int in_size = analyzer_->GetBufferSize();

    // Resize output vector if necessary
    if (int out_size = analyzer_->GetOutputSize(); output.size() != out_size) {
      output.resize(out_size);
    }

    auto command = sync_data_.Pop();

    switch (command) {
      case Command::Analyze: {
        // Get input data, run FFT and update local cache
        // P.S.: do not log this because this command is received too often
        input = sync_data_.GetBuffer(in_size);
        analyzer_->Execute(input.data(), static_cast<int>(input.size()), output.data());

        // Do not normalize output vector, just set 1.0 as maximum value
        std::for_each(output.begin(), output.end(), [](double& value) {
          if (value > 1.0) value = 1.0;
        });

        // When a new song starts, let bars rise smoothly instead of jumping to their height
        if (fade_in_pending_.exchange(false)) {
          fade_in_start = std::chrono::steady_clock::now();
        }

        if (fade_in_start.has_value()) {
          const double progress =
              std::chrono::duration<double>(std::chrono::steady_clock::now() - *fade_in_start) /
              kFadeInDuration;

          if (progress >= 1.0) {
            fade_in_start.reset();
          } else {
            // Smoothstep curve: bars start rising slowly and settle smoothly in their height
            const double gain = progress * progress * (3.0 - (2.0 * progress));
            std::for_each(output.begin(), output.end(), [gain](double& value) { value *= gain; });
          }
        }

        previous = output;

        auto dispatcher = GetDispatcher();
        if (!dispatcher) break;

        // Send result to UI
        auto event = interface::CustomEvent::DrawAudioSpectrum(output);
        dispatcher->SendEvent(event);

      } break;

      case Command::RunClearAnimation: {
        LOG("Analysis handler received command to run clear animation on audio visualizer");
        ProcessClearAnimation(previous);

      } break;

      case Command::RunRegainAnimation: {
        LOG("Analysis handler received command to run regain animation on audio visualizer");
        ProcessRegainAnimation(previous);
      } break;

      default:
        break;
    }
  }

  LOG("Finish analysis handler thread");
  finished_ = true;
}

/* ********************************************************************************************** */

void MediaController::NotifyFileSelection(const std::filesystem::path& filepath) {
  auto player = GetPlayer();
  if (!player) return;

  player->Play(filepath);
}

/* ********************************************************************************************** */

void MediaController::Pause() {
  auto player = GetPlayer();
  if (!player) return;

  player->PauseOrResume();
}

/* ********************************************************************************************** */

void MediaController::Resume(bool run_animation) {
  if (run_animation) {
    // Do not toggle player right away, let thread run its animation first
    sync_data_.Push(Command::RunRegainAnimation);
  } else {
    auto player = GetPlayer();
    if (!player) return;

    player->PauseOrResume();
  }
}

/* ********************************************************************************************** */

void MediaController::Stop() {
  auto player = GetPlayer();
  if (!player) return;

  player->Stop();
}

/* ********************************************************************************************** */

void MediaController::SetVolume(model::Volume value) {
  UpdateStatus([&value](model::PlayerStatus& status) { status.volume = value; });

  auto player = GetPlayer();
  if (!player) return;

  player->SetAudioVolume(value);
}

/* ********************************************************************************************** */

void MediaController::ResizeAnalysisOutput(int value) {
  std::scoped_lock lock(sync_data_.mutex);
  analyzer_->Init(value);
}

/* ********************************************************************************************** */

void MediaController::SeekForwardPosition(int value) {
  auto player = GetPlayer();
  if (!player) return;

  player->SeekForwardPosition(value);
}

/* ********************************************************************************************** */

void MediaController::SeekBackwardPosition(int value) {
  auto player = GetPlayer();
  if (!player) return;

  player->SeekBackwardPosition(value);
}

/* ********************************************************************************************** */

void MediaController::ApplyAudioFilters(const model::EqualizerPreset& filters) {
  auto player = GetPlayer();
  if (!player) return;

  player->ApplyAudioFilters(filters);
}

/* ********************************************************************************************** */

void MediaController::NotifyPlaylistSelection(const model::Playlist& playlist) {
  auto player = GetPlayer();
  if (!player) return;

  player->Play(playlist);
}

/* ********************************************************************************************** */

void MediaController::NotifyErrorDialogClosed() {
  auto player = GetPlayer();
  if (!player) return;

  player->DequeueNextSong();
}

/* ********************************************************************************************** */

void MediaController::SkipToNextSong() {
  auto player = GetPlayer();
  if (!player) return;

  player->SkipToNext();
}

/* ********************************************************************************************** */

void MediaController::SkipToPreviousSong() {
  auto player = GetPlayer();
  if (!player) return;

  player->SkipToPrevious();
}

/* ********************************************************************************************** */

void MediaController::SetRepeatMode(model::RepeatMode mode) {
  UpdateStatus([mode](model::PlayerStatus& status) { status.repeat = mode; });

  auto player = GetPlayer();
  if (!player) return;

  player->SetRepeatMode(mode);
}

/* ********************************************************************************************** */

void MediaController::SetShuffle(bool enabled) {
  UpdateStatus([enabled](model::PlayerStatus& status) { status.shuffle = enabled; });

  auto player = GetPlayer();
  if (!player) return;

  player->SetShuffle(enabled);
}

/* ********************************************************************************************** */

void MediaController::SetAudioDevice(const std::string& device) {
  auto player = GetPlayer();
  if (!player) return;

  player->SetAudioDevice(device);
}

/* ********************************************************************************************** */

model::AudioDevices MediaController::GetAudioDevices() {
  auto player = GetPlayer();
  if (!player) return {};

  return player->GetAudioDevices();
}

/* ********************************************************************************************** */

void MediaController::ClearSongInformation(bool playing) {
  if (playing) sync_data_.Push(Command::RunClearAnimation);

  // Settings chosen by user are kept, as they do not depend on song
  UpdateStatus([](model::PlayerStatus& status) {
    status.state = model::Song::MediaState::Empty;
    status.artist.clear();
    status.title.clear();
    status.position = 0;
    status.duration = 0;
    status.output.reset();
  });

  auto dispatcher = GetDispatcher();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::ClearSongInfo();

  // Notify all blocks to clear info about song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::NotifySongInformation(const model::Song& info) {
  // Bars must rise smoothly when the new song starts
  fade_in_pending_ = true;

  UpdateStatus([&info](model::PlayerStatus& status) {
    // Song starts playing right after being loaded
    status.state = model::Song::MediaState::Play;
    status.artist = info.artist;
    status.position = 0;
    status.duration = info.duration;

    // Without a title in metadata, use the same as UI to identify song
    status.title = !info.title.empty()            ? info.title
                   : info.stream_info.has_value() ? info.stream_info->base_url
                                                  : info.filepath.filename().string();
  });

  auto dispatcher = GetDispatcher();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::UpdateSongInfo(info);

  // Notify all blocks with information about the recently loaded song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::NotifySongState(const model::Song::CurrentInformation& curr_info) {
  UpdateStatus([&curr_info](model::PlayerStatus& status) {
    status.state = curr_info.state;
    status.position = curr_info.position;
  });

  if (curr_info.state == model::Song::MediaState::Pause ||
      curr_info.state == model::Song::MediaState::Finished) {
    // Enqueue animation to thread
    sync_data_.Push(Command::RunClearAnimation);
  }

  auto dispatcher = GetDispatcher();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::UpdateSongState(curr_info);

  // Notify Audio Player block with new state information about the current song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::SendAudioRaw(const int16_t* buffer, int size) {
  // Append audio data to be analyzed by thread
  sync_data_.Append(buffer, size);
}

/* ********************************************************************************************** */

void MediaController::NotifyError(error::Code code, const std::string& detail) {
  auto dispatcher = GetDispatcher();
  if (!dispatcher) return;

  // Notify Terminal about error that has occurred in Audio thread
  dispatcher->SetApplicationError(code, detail);
}

/* ********************************************************************************************** */

void MediaController::NotifyAudioOutput(const model::AudioOutput& output) {
  UpdateStatus([&output](model::PlayerStatus& status) { status.output = output; });

  auto dispatcher = GetDispatcher();
  if (!dispatcher) return;

  // Notify all blocks with audio output used to play current song
  dispatcher->SendEvent(interface::CustomEvent::UpdateAudioOutput(output));
}

/* ********************************************************************************************** */

void MediaController::ProcessClearAnimation(const std::vector<double>& data) {
  auto dispatcher = GetDispatcher();
  if (!dispatcher) return;

  using namespace std::chrono_literals;

  std::vector<double> bars(data);

  for (double i = 0; i < 80; i++) {
    // Number of bars may be changed in the meantime (e.g. terminal is resized), and these bars
    // are not the ones expected by UI anymore, so just cancel animation
    if (static_cast<int>(bars.size()) != analyzer_->GetOutputSize()) break;

    // Each time this loop is executed, it will reduce spectrum bar values to 75% based on its
    // previous values (this value was decided based on feeling :P)
    std::transform(bars.begin(), bars.end(), bars.begin(), [](double x) {
      double value = x * 0.75;
      return value > 0.001 ? value : 0.001;
    });

    // Send result to UI
    auto event = interface::CustomEvent::DrawAudioSpectrum(bars);
    dispatcher->SendEvent(event);

    // Sleep a little bit before sending a new update to UI. And in case of receiving a new
    // command in the meantime, just cancel animation
    auto timeout = std::chrono::system_clock::now() + 0.03s;
    if (bool exit_animation = sync_data_.WaitForCommandOrUntil(timeout); exit_animation) break;
  }

  // Always finish with the number of bars currently expected by UI
  bars = std::vector(static_cast<size_t>(analyzer_->GetOutputSize()), 0.001);
  auto event = interface::CustomEvent::DrawAudioSpectrum(bars);
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::ProcessRegainAnimation(const std::vector<double>& data) {
  static constexpr int kStep = 20;
  auto dispatcher = GetDispatcher();
  if (!dispatcher) return;

  using namespace std::chrono_literals;

  std::vector<double> bars;
  bars.reserve(data.size());

  for (double i = 1; i <= kStep; i++) {
    // Number of bars may be changed while song was paused (e.g. terminal is resized), and these
    // bars are not the ones expected by UI anymore, so just skip animation
    if (static_cast<int>(data.size()) != analyzer_->GetOutputSize()) break;

    // Each time this loop is executed, it will increase spectrum bar values in a step of 1/20
    // based on its previous values (this value was also decided based on feeling)
    for (const auto& value : data) bars.push_back(value * (i / kStep));

    // Send result to UI
    auto event = interface::CustomEvent::DrawAudioSpectrum(bars);
    dispatcher->SendEvent(event);

    // Sleep a little bit before sending a new update to UI. And in case of receiving a new
    // command in the meantime, just cancel animation
    auto timeout = std::chrono::system_clock::now() + 0.015s;
    if (bool exit_animation = sync_data_.WaitForCommandOrUntil(timeout); exit_animation) break;

    bars.clear();
  }

  // Give some time until analyzer gets back on track
  auto timeout = std::chrono::system_clock::now() + 0.03s;
  sync_data_.WaitForCommandOrUntil(timeout);

  // FIX: This is not good, but it was the way found to send a command from thread
  auto event = interface::CustomEvent::ResumeSong(/*run_animation=*/false);
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

std::shared_ptr<interface::EventDispatcher> MediaController::GetDispatcher() const {
  // Do not throw if it fails: this happens while application is exiting (after interface is
  // destroyed), so caller simply skips its notification
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) WARN("Cannot lock event dispatcher");

  return dispatcher;
}

/* ********************************************************************************************** */

std::shared_ptr<audio::AudioControl> MediaController::GetPlayer() const {
  // Same as dispatcher, this happens while application is exiting, so caller skips its command
  auto player = player_ctl_.lock();
  if (!player) WARN("Cannot lock audio player, command will be discarded");

  return player;
}

}  // namespace middleware
