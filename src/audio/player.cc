#include "audio/player.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>

#ifndef SPECTRUM_DEBUG
#include "audio/driver/alsa.h"
#include "audio/driver/ffmpeg.h"
#include "web/driver/ytdlp_wrapper.h"
#else
#include "debug/dummy_decoder.h"
#include "debug/dummy_playback.h"
#include "debug/dummy_stream_fetcher.h"
#endif

#include "view/base/notifier.h"

namespace audio {

std::shared_ptr<Player> Player::Create(bool verbose, const std::string& device,
                                       audio::Playback* playback, audio::Decoder* decoder,
                                       web::StreamFetcher* fetcher, bool asynchronous) {
  LOG("Create new instance of player");

#ifndef SPECTRUM_DEBUG
  // Create playback object
  auto pb = playback != nullptr ? std::unique_ptr<audio::Playback>(std::move(playback))
                                : std::make_unique<driver::Alsa>();

  // Create decoder object
  auto dc = decoder != nullptr ? std::unique_ptr<audio::Decoder>(std::move(decoder))
                               : std::make_unique<driver::FFmpeg>(verbose);

  // Create fetcher object
  auto ft = fetcher != nullptr ? std::unique_ptr<web::StreamFetcher>(std::move(fetcher))
                               : std::make_unique<driver::YtDlpWrapper>();

#else
  // Create playback object
  auto pb = std::make_unique<driver::DummyPlayback>();

  // Create decoder object
  auto dc = std::make_unique<driver::DummyDecoder>();

  // Create fetcher object
  auto ft = std::make_unique<driver::DummyStreamFetcher>();
#endif

  // Simply extend the Player class, as we do not want to expose the default constructor,
  // neither do we want to use std::make_shared explicitly calling operator new()
  struct MakeSharedEnabler : public Player {
    explicit MakeSharedEnabler(std::unique_ptr<audio::Playback>&& playback,
                               std::unique_ptr<audio::Decoder>&& decoder,
                               std::unique_ptr<web::StreamFetcher>&& fetcher)
        : Player(std::move(playback), std::move(decoder), std::move(fetcher)) {}
  };

  // Instantiate Player
  auto player = std::make_shared<MakeSharedEnabler>(std::move(pb), std::move(dc), std::move(ft));

  // Initialize internal components
  player->Init(asynchronous, device);

  return player;
}

/* ********************************************************************************************** */

Player::Player(std::unique_ptr<audio::Playback>&& playback,
               std::unique_ptr<audio::Decoder>&& decoder,
               std::unique_ptr<web::StreamFetcher>&& fetcher)
    : playback_{std::move(playback)}, decoder_{std::move(decoder)}, fetcher_{std::move(fetcher)} {}

/* ********************************************************************************************** */

Player::~Player() {
  try {
    Exit();
  } catch (...) {
    // We don't mind about exceptions at this point in life
  }

  if (audio_loop_.joinable()) {
    audio_loop_.join();
  }
}

/* ********************************************************************************************** */

void Player::Init(bool asynchronous, const std::string& device) {
  LOG("Initialize player with async=", asynchronous, " device=", std::quoted(device));
  finished_ = false;
  device_ = device;

  // Open playback stream and configure desired parameters for playback
  error::Code result = CreatePlaybackStream(device_);

  // Device chosen by user may not be available anymore (e.g. it was disconnected)
  if (result != error::kSuccess && !device_.empty()) {
    WARN("Cannot use output device chosen by user, device=", std::quoted(device_));
    failed_device_ = std::exchange(device_, "");
    result = CreatePlaybackStream(device_);
  }

  if (result != error::kSuccess) {
    throw std::runtime_error("Cannot initialize playback stream in player");
  }

  if (asynchronous) {
    // Spawn thread for Audio player
    audio_loop_ = std::thread(&Player::AudioHandler, this);
  }
}

/* ********************************************************************************************** */

void Player::ResetMediaControl(error::Code result, bool error_parsing) {
  LOG("Reset media control with error code=", result);
  bool notify_finished = media_control_.state == State::Play;

  // Keep song file name, in case of error it is shown to user
  const std::string filename = curr_song_ ? curr_song_->filepath.filename().string() : "";

  // Clear internal data
  decoder_->ClearCache();
  media_control_.Reset();
  curr_song_.reset();

  // Song was stopped to skip to another one from playlist
  if (pending_skip_.has_value()) {
    media_control_.Push(*pending_skip_);
    pending_skip_.reset();
  }

  auto media_notifier = notifier_.lock();
  if (!media_notifier) return;

  if (result != error::kSuccess) {
    // Song information was already sent to UI (error happened while decoding), so clear it,
    // otherwise UI would keep showing information about a song that is not playing anymore
    if (!error_parsing) media_notifier->ClearSongInformation(true);

    // Site is refusing requests, so every other song from URL would also fail (and asking for
    // them could make it refuse requests for even longer)
    if (result == error::kStreamBlocked) {
      WARN("Stop playlist, as requests for songs from URL are being refused");
      curr_playlist_.reset();
      failed_songs_ = 0;
    }

    // In case of error, notify about it
    media_notifier->NotifyError(result, filename);

    // Warning is not shown in a dialog (whose closing would play next song), so keep playing
    if (error::ApplicationError::GetLevel(result) == error::Level::Warning) {
      // But do not try every remaining song when they keep failing (e.g. no network to stream them)
      bool has_next_song = CanSkip(Command::SkipToNext());

      if (has_next_song && ++failed_songs_ >= kMaxFailedSongs) {
        WARN("Stop playlist, as ", failed_songs_.load(), " songs failed in a row");
        curr_playlist_.reset();
        failed_songs_ = 0;
        media_notifier->NotifyError(error::kTooManyFailedSongs, "");
      } else if (curr_playlist_) {
        // Do not repeat this song (even if repeat mode is set to it), as it would fail again
        media_control_.Push(Command::SkipToNext());
      }
    }

    return;
  }

  // If last state was "playing", it means we should notify that song has finished successfully
  if (notify_finished) {
    media_notifier->NotifySongState(
        model::Song::CurrentInformation{.state = model::Song::MediaState::Finished});
  }

  // Clear any song information from UI
  media_notifier->ClearSongInformation(!error_parsing);

  // Play next song only if this one has finished (otherwise, it was stopped or replaced by user)
  if (notify_finished) DequeueNextSongFromPlaylist();
}

/* ********************************************************************************************** */

bool Player::HandleCommand(void* buffer, int size, void* analysis, int analysis_size,
                           int64_t& new_position, int& last_position) {
  auto command = media_control_.Pop();
  auto media_notifier = notifier_.lock();

  // Format of samples in buffer (it is not the one expected by playback anymore when command
  // changes output device to one that does not support it)
  const model::AudioFormat buffer_format = format_;

  if (media_control_.state == State::Stop || media_control_.state == State::Exit) {
    return false;
  }

  using Cmd = Command::Identifier;

  switch (command.GetId()) {
    case Command::Identifier::Play: {
      LOG("Audio handler received command requesting to play a new song");
      // Add play request back to queue
      media_control_.Push(command);

      // Stop current song
      media_control_.state = State::Stop;
      playback_->Stop();
      return false;
    } break;

    case Command::Identifier::PauseOrResume: {
      INFO("Audio handler received command to pause song");
      media_control_.state = TranslateCommand(command);
      playback_->Pause();

      // As this thread can stay blocked for a long time, waiting for a command,
      // notify state to media controller
      if (media_notifier) {
        media_notifier->NotifySongState(model::Song::CurrentInformation{
            .state = model::Song::MediaState::Pause,
            .position = (uint32_t)last_position,
        });
      }

      // Block thread until receives one of the informed commands (ignoring skip commands when there
      // is no song to skip to)
      bool keep_executing = false;
      Command command_after_wait = Command::None();

      bool keep_waiting = false;

      do {
        keep_executing =
            media_control_.WaitFor(Cmd::Play, Cmd::PauseOrResume, Cmd::Stop, Cmd::SkipToNext,
                                   Cmd::SkipToPrevious, Cmd::SetDevice);
        command_after_wait = media_control_.Pop();

        bool change_device = keep_executing && command_after_wait == Cmd::SetDevice;
        bool skip =
            command_after_wait == Cmd::SkipToNext || command_after_wait == Cmd::SkipToPrevious;

        // Output device may be changed while song is paused
        if (change_device) ChangeDevice(command_after_wait.GetContent<std::string>());

        keep_waiting = keep_executing && (change_device || (skip && !CanSkip(command_after_wait)));
      } while (keep_waiting);

      // Received command different from PauseOrResume
      if (!keep_executing || command_after_wait != Cmd::PauseOrResume) {
        INFO("Audio handler received command to ", command_after_wait);

        bool play_new_song = command_after_wait == Cmd::Play ||
                             command_after_wait == Cmd::SkipToNext ||
                             command_after_wait == Cmd::SkipToPrevious;

        // Stop current song (if interrupted by a new song, it must not be notified as finished)
        media_control_.state = play_new_song ? State::Stop : TranslateCommand(command_after_wait);

        if (command_after_wait == Cmd::Play) {
          LOG("Re-adding command to play new song in the queue");
          media_control_.Push(command_after_wait);
        } else if (play_new_song) {
          // Skip request selects the song to play after stopping this one
          pending_skip_ = command_after_wait;
        }

        // User stopped playing, so there is no next song to play
        if (command_after_wait == Cmd::Stop) curr_playlist_.reset();

        playback_->Stop();
        return false;
      }

      INFO("Audio handler received command to resume song");
      media_control_.state = State::Play;
      playback_->Prepare();
    } break;

    case Command::Identifier::SkipToNext:
    case Command::Identifier::SkipToPrevious: {
      if (!CanSkip(command)) {
        LOG("Audio handler received command to ", command, ", but there is no song to skip to");
        break;
      }

      INFO("Audio handler received command to ", command);
      // Skip request selects the song to play after stopping this one
      pending_skip_ = command;

      // Stop current song
      media_control_.state = State::Stop;
      playback_->Stop();
      return false;
    } break;

    case Command::Identifier::Stop:
    case Command::Identifier::Exit: {
      INFO("Audio handler received command to ", command);
      media_control_.state = TranslateCommand(command);
      playback_->Stop();

      // User stopped playing, so there is no next song to play
      if (command == Command::Identifier::Stop) curr_playlist_.reset();
      return false;
    } break;

    case Command::Identifier::SeekForward: {
      int offset = command.GetContent<int>();
      LOG("Audio handler received command to seek forward with value=", offset);

      if ((new_position + offset) < curr_song_->duration) {
        new_position += offset;
        return true;
      }
    } break;

    case Command::Identifier::SeekBackward: {
      int offset = command.GetContent<int>();
      LOG("Audio handler received command to seek backward with value=", offset);

      if (new_position > 0 && (new_position - offset) >= 0) {
        new_position -= offset;
        return true;
      }
    } break;

    case Command::Identifier::SetVolume: {
      model::Volume value = command.GetContent<model::Volume>();
      LOG("Audio handler received command to set volume with value=", value);
      decoder_->SetVolume(value);
    } break;

    case Command::Identifier::SetDevice: {
      LOG("Audio handler received command to change output device");
      ChangeDevice(command.GetContent<std::string>());
    } break;

    case Command::Identifier::UpdateAudioFilters: {
      model::EqualizerPreset value = command.GetContent<model::EqualizerPreset>();
      LOG("Audio handler received command to update audio filters");

      // Song keeps playing with previous filters, but let user know about it
      if (auto result = decoder_->UpdateFilters(value); result != error::kSuccess) {
        ERROR("Cannot update audio filters, error=", result);
        if (auto media_notifier = notifier_.lock(); media_notifier) {
          media_notifier->NotifyError(result, "");
        }
      }
    } break;

    default:
      break;
  }

  // Samples in buffer cannot be played by the new output device, so discard them and ask decoder
  // for samples in the format that it expects
  if (format_ != buffer_format) {
    LOG("Format expected by playback has changed from ", buffer_format, " to ", format_);

    if (auto result = decoder_->SetOutputFormat(format_); result != error::kSuccess) {
      ERROR("Cannot change output format on decoder, stop playing song, error=", result);
      playback_error_ = result;
      return false;
    }

    return true;
  }

  // Send raw information to media controller to run audio analysis
  if (media_notifier) {
    // Analysis must use samples not affected by volume (if available), so spectrum visualizer
    // keeps working even when audio is muted. Otherwise, samples sent to playback may be used, but
    // only when they are in the format expected by analysis
    if (analysis != nullptr) {
      media_notifier->SendAudioRaw(static_cast<const int16_t*>(analysis),
                                   analysis_size * kNumberChannels);
    } else if (format_ == kAnalysisFormat) {
      media_notifier->SendAudioRaw(static_cast<const int16_t*>(buffer), size * kNumberChannels);
    }
  }

  // Write samples to playback (stop playing song if it fails, e.g. output device disconnected)
  if (auto result = playback_->AudioCallback(buffer, size); result != error::kSuccess) {
    ERROR("Cannot write samples to playback, stop playing song, error=", result);
    playback_error_ = result;
    return false;
  }

  // Notify song state to graphical interface
  if (last_position != new_position) {
    last_position = static_cast<int>(new_position);

    if (media_notifier) {
      media_notifier->NotifySongState(model::Song::CurrentInformation{
          .state = model::Song::MediaState::Play,
          .position = (uint32_t)last_position,
      });
    }
  }

  return true;
}

/* ********************************************************************************************** */

void Player::AudioHandler() {
  util::Logger::SetThreadName("audio");
  LOG("Start audio handler thread");
  fetcher_->Init();

  using Cmd = Command::Identifier;

  // Block this thread until UI informs us a song to play
  while (media_control_.WaitFor(Cmd::Play, Cmd::SkipToNext, Cmd::SkipToPrevious, Cmd::PlayNext,
                                Cmd::SetDevice)) {
    auto command = media_control_.Pop();

    // Output device may be changed while there is no song playing
    if (command == Cmd::SetDevice) {
      ChangeDevice(command.GetContent<std::string>());
      continue;
    }

    // Select song to play (if any)
    auto song = SelectSong(command);
    if (!song.has_value()) continue;

    // Update internal media state and initialize current song
    media_control_.state = State::Play;
    curr_song_ = std::make_unique<model::Song>(std::move(*song));
    error::Code result = error::kSuccess;
    // Song information is only filled after opening it, so just let user know where it comes from
    LOG("Audio handler received new song to play from ", curr_song_->stream_info
                                                             ? curr_song_->stream_info->base_url
                                                             : curr_song_->filepath.string());

    // Get streaming information if song contains a valid URL
    if (curr_song_->stream_info.has_value()) result = fetcher_->ExtractInfo(*curr_song_);

    // Attempt to parse song (file may not have a supported extension or failed to fetch URL)
    if (result == error::kSuccess) result = decoder_->Open(*curr_song_);

    // Playback is asked to use the format from song, and decoder must create samples in the format
    // expected by playback (which depends on what is supported by output device)
    if (result == error::kSuccess) result = ConfigureOutput(*curr_song_);

    // In case of error, reset media controls and notify terminal UI with error
    if (result != error::kSuccess) {
      ResetMediaControl(result, /* error_parsing= */ true);
      continue;  // we don't wanna keep in this loop anymore, so wait for next song!
    }

    failed_songs_ = 0;
    // Full path is logged only here (other messages refer to its filename)
    INFO("Playing song=", *curr_song_,
         curr_song_->stream_info ? "" : " path=" + curr_song_->filepath.string());

    {
      // Otherwise, it is a supported audio extension, send detailed audio information to UI
      if (auto media_notifier = notifier_.lock(); media_notifier) {
        // Notify interface about new song
        media_notifier->NotifySongInformation(*curr_song_);
        NotifyAudioOutput();
      }
    }

    // Inform playback driver to be ready to play
    playback_->Prepare();

    int position = -1;  // in seconds

    // To keep decoding audio, return true in lambda function
    result = decoder_->Decode(
        period_size_ / 2, [this, &position](void* buffer, int size, void* analysis,
                                            int analysis_size, int64_t& new_position) {
          return HandleCommand(buffer, size, analysis, analysis_size, new_position, position);
        });

    // Decoding stops without error when playback fails, so report it from here
    if (result == error::kSuccess) result = std::exchange(playback_error_, error::kSuccess);

    // Reached end of song, this may be originated from one of these situations:
    //  1. naturally;
    //  2. forced to stop/exit by user;
    //  3. error from fetching streaming info;
    //  4. error from decoding;
    //  5. error from playback;
    ResetMediaControl(result);
  }

  LOG("Finish audio handler thread");
  fetcher_->Finish();
  finished_ = true;
}

/* ********************************************************************************************** */

void Player::DequeueNextSongFromPlaylist() {
  if (!curr_playlist_) return;

  // Song is selected only when command is handled (or playlist is cleared, if there is no next
  // song)
  media_control_.Push(Command::PlayNext());
}

/* ********************************************************************************************** */

void Player::ApplyShuffle() {
  if (!curr_playlist_ || shuffled_ == shuffle_) return;
  shuffled_ = shuffle_;

  if (shuffled_) {
    LOG("Shuffle next songs from playlist");
    std::shuffle(order_.begin() + static_cast<std::ptrdiff_t>(curr_position_) + 1, order_.end(),
                 random_engine_);
    return;
  }

  // Back to original order, continuing from current song
  LOG("Restore original order of songs from playlist");
  std::size_t current = order_[curr_position_];
  std::iota(order_.begin(), order_.end(), 0);
  curr_position_ = current;
}

/* ********************************************************************************************** */

std::optional<model::Song> Player::SelectSong(const Command& command) {
  switch (command.GetId()) {
    case Command::Identifier::Play: {
      // Single song is played as a queue containing only itself (so it can be repeated)
      auto playlist =
          std::holds_alternative<model::Song>(command.content)
              ? model::Playlist{.index = -1, .songs = {command.GetContent<model::Song>()}}
              : command.GetContent<model::Playlist>();

      if (playlist.IsEmpty()) {
        ERROR("Received playlist without any song to play");
        return std::nullopt;
      }

      INFO("Start playing playlist=", playlist);
      curr_playlist_ = std::move(playlist);
      order_.resize(curr_playlist_->songs.size());
      std::iota(order_.begin(), order_.end(), 0);
      curr_position_ = 0;
      shuffled_ = false;
    } break;

    case Command::Identifier::PlayNext:
      // Current song has finished, so play it again
      if (curr_playlist_ && repeat_ == model::RepeatMode::One) break;
      [[fallthrough]];

    case Command::Identifier::SkipToNext: {
      if (!CanSkip(Command::SkipToNext())) {
        // No need to keep this anymore, so reset it
        if (curr_playlist_) LOG("Clearing playlist, as it does not contain any other song");
        curr_playlist_.reset();
        return std::nullopt;
      }

      // Wrap around (when last song from playlist is reached, repeat mode is set to all songs)
      curr_position_ = (curr_position_ + 1) % order_.size();
    } break;

    case Command::Identifier::SkipToPrevious: {
      if (!CanSkip(command)) return std::nullopt;

      // On first song, simply play it again
      if (curr_position_ > 0) --curr_position_;
    } break;

    default:
      return std::nullopt;
  }

  // Shuffle next songs, if enabled
  ApplyShuffle();

  std::size_t index = order_[curr_position_];
  LOG("Select song from playlist at position=", curr_position_, " (index=", index, ")");

  model::Song song = curr_playlist_->songs[index];
  if (!curr_playlist_->name.empty()) song.playlist = curr_playlist_->name;

  return song;
}

/* ********************************************************************************************** */

bool Player::CanSkip(const Command& command) {
  if (!curr_playlist_) return false;

  // Shuffle may have been enabled/disabled in the meantime
  ApplyShuffle();

  // Previous is always possible (on first song, it is played again)
  if (command == Command::Identifier::SkipToPrevious) return true;

  return command == Command::Identifier::SkipToNext &&
         (curr_position_ + 1 < order_.size() || repeat_ == model::RepeatMode::All);
}

/* ********************************************************************************************** */

void Player::ChangeDevice(const std::string& device) {
  INFO("Change output device to ", std::quoted(device));

  if (error::Code result = CreatePlaybackStream(device); result != error::kSuccess) {
    ERROR("Cannot change output device, error=", result);

    // Previous playback stream was already released, so create it again
    if (CreatePlaybackStream(device_) != error::kSuccess) {
      ERROR("Cannot use previous output device");
    }

    if (auto media_notifier = notifier_.lock(); media_notifier) {
      media_notifier->NotifyError(error::kOpenDeviceFailed, device);
    }

    return;
  }

  device_ = device;

  // Current song (if any) is played by another device, which may not expect the same format
  if (curr_song_) NotifyAudioOutput();
}

/* ********************************************************************************************** */

error::Code Player::CreatePlaybackStream(const std::string& device) {
  error::Code result = playback_->CreatePlaybackStream(device);
  return result == error::kSuccess ? ConfigurePlayback() : result;
}

/* ********************************************************************************************** */

void Player::NotifyAudioOutput() {
  auto media_notifier = notifier_.lock();
  if (!media_notifier) return;

  media_notifier->NotifyAudioOutput(
      model::AudioOutput{.device = playback_->GetDevice(), .format = format_});
}

/* ********************************************************************************************** */

error::Code Player::ConfigurePlayback() {
  error::Code result = playback_->ConfigureParameters(desired_format_);
  if (result != error::kSuccess) return result;

  // Device may not support it, so decoder must create samples in the format that it expects
  format_ = playback_->GetFormat();

  // This value is used to decide buffer size for song decoding
  period_size_ = static_cast<int>(playback_->GetPeriodSize());

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Player::ConfigureOutput(const model::Song& song) {
  model::AudioFormat desired = desired_format_;

  // Sample rate may not be known (in this case, keep using the one from the last song)
  if (song.sample_rate > 0) desired.sample_rate = song.sample_rate;

  // Playback stream is configured again only when needed, as songs played in a row usually have
  // the same format (e.g. the ones from an album)
  if (desired != desired_format_) {
    INFO("Change desired format from ", desired_format_, " to ", desired);
    const model::AudioFormat previous = std::exchange(desired_format_, desired);

    if (error::Code result = ConfigurePlayback(); result != error::kSuccess) {
      ERROR("Cannot configure playback with desired format, error=", result);

      // Playback stream cannot be used anymore, so create it again with the previous format
      desired_format_ = previous;

      if (CreatePlaybackStream(device_) != error::kSuccess) {
        ERROR("Cannot create playback stream again");
      }

      return result;
    }
  }

  return decoder_->SetOutputFormat(format_);
}

/* ********************************************************************************************** */

void Player::RegisterInterfaceNotifier(const std::shared_ptr<interface::Notifier>& notifier) {
  LOG("Register new interface notifier");
  notifier_ = notifier;

  // Now it is possible to let user know about device that could not be used on initialization
  if (notifier && !failed_device_.empty()) {
    notifier->NotifyError(error::kOpenDeviceFailed, std::exchange(failed_device_, ""));
  }
}

/* ********************************************************************************************** */

void Player::Play(const std::filesystem::path& filepath) {
  LOG("Add command to queue: \"Play\" (filepath=", std::quoted(filepath.string()), ")");
  media_control_.Push(Command::Play(model::Song{.filepath = filepath}));
  failed_songs_ = 0;
}

/* ********************************************************************************************** */

void Player::Play(const model::Playlist& playlist) {
  LOG("Add command to queue: \"Play\" (playlist=", playlist, ")");
  media_control_.Push(Command::Play(playlist));
  failed_songs_ = 0;
}

/* ********************************************************************************************** */

void Player::PauseOrResume() {
  LOG("Add command to queue: ",
      std::quoted(media_control_.state == State::Play ? "Pause" : "Resume"));
  media_control_.Push(Command::PauseOrResume());
}

/* ********************************************************************************************** */

void Player::Stop() {
  LOG("Add command to queue: Stop");
  media_control_.Push(Command::Stop());
}

/* ********************************************************************************************** */

void Player::SetAudioVolume(const model::Volume& value) {
  INFO("Set audio volume with value=", value);

  // Set volume direcly or add new command to audio queue, based on current media state
  switch (media_control_.state) {
    // If state is idle, there is no music playing
    case State::Idle: {
      error::Code result = decoder_->SetVolume(value);

      // Notify error
      if (result != error::kSuccess) {
        auto media_notifier = notifier_.lock();
        if (media_notifier) {
          media_notifier->NotifyError(result, "");
        }
      }
    } break;

    // Otherwise, add command to queue
    case State::Play:
    case State::Pause:
    case State::Stop:
      media_control_.Push(Command::SetVolume(value));
      break;

    default:
      break;
  }
}

/* ********************************************************************************************** */

model::Volume Player::GetAudioVolume() const {
  LOG("Get audio volume");
  return decoder_->GetVolume();
}

/* ********************************************************************************************** */

void Player::SeekForwardPosition(int value) {
  LOG("Add command to queue: \"SeekForward\" (with value=", value, ")");
  media_control_.Push(Command::SeekForward(value));
}

/* ********************************************************************************************** */

void Player::SeekBackwardPosition(int value) {
  LOG("Add command to queue: \"SeekBackward\" (with value=", value, ")");
  media_control_.Push(Command::SeekBackward(value));
}

/* ********************************************************************************************** */

void Player::ApplyAudioFilters(const model::EqualizerPreset& filters) {
  LOG("Apply updated audio filters");

  // Set audio filters direcly or add new command to audio queue, based on current media state
  switch (media_control_.state) {
    // If state is idle, there is no music playing
    case State::Idle: {
      error::Code result = decoder_->UpdateFilters(filters);

      // Notify error
      if (result != error::kSuccess) {
        auto media_notifier = notifier_.lock();
        if (media_notifier) {
          media_notifier->NotifyError(result, "");
        }
      }
    } break;

    // Otherwise, add command to queue
    case State::Play:
    case State::Pause:
    case State::Stop:
      media_control_.Push(Command::UpdateAudioFilters(filters));
      break;

    default:
      break;
  }
}

/* ********************************************************************************************** */

void Player::DequeueNextSong() {
  // Error may not have stopped current song (e.g. failed to update audio filters), so keep it
  if (media_control_.state != State::Idle) {
    LOG("Song is still playing, do not dequeue next song from playlist");
    return;
  }

  LOG("Add command to queue: \"SkipToNext\" (to dequeue next song from playlist)");
  media_control_.Push(Command::SkipToNext());
}

/* ********************************************************************************************** */

void Player::SkipToNext() {
  LOG("Add command to queue: \"SkipToNext\"");
  media_control_.Push(Command::SkipToNext());
}

/* ********************************************************************************************** */

void Player::SkipToPrevious() {
  LOG("Add command to queue: \"SkipToPrevious\"");
  media_control_.Push(Command::SkipToPrevious());
}

/* ********************************************************************************************** */

void Player::SetRepeatMode(model::RepeatMode mode) {
  INFO("Set repeat mode=", mode);
  repeat_ = mode;
}

/* ********************************************************************************************** */

void Player::SetShuffle(bool enabled) {
  INFO("Set shuffle=", enabled);
  shuffle_ = enabled;
}

/* ********************************************************************************************** */

void Player::SetAudioDevice(const std::string& device) {
  LOG("Add command to queue: \"SetDevice\" (device=", std::quoted(device), ")");
  media_control_.Push(Command::SetDevice(device));
}

/* ********************************************************************************************** */

model::AudioDevices Player::GetAudioDevices() const {
  LOG("Get audio devices");
  return playback_->ListDevices();
}

/* ********************************************************************************************** */

void Player::Exit() {
  if (finished_) {
    // Player already exited from audio loop
    return;
  }

  LOG("Add command to queue: \"Exit\"");
  media_control_.Push(Command::Exit());
}

}  // namespace audio
