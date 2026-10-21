#include "util/mpris.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

#if __has_include(<systemd/sd-bus.h>)
#include <systemd/sd-bus.h>
#else
#include <basu/sd-bus.h>
#endif

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <string_view>
#include <vector>

#include "model/mpris.h"
#include "util/logger.h"

namespace util {

namespace {

constexpr int kInvalidFd = -1;                  //!< File descriptor not opened
constexpr std::size_t kDiscardBufferSize = 64;  //!< Size of each chunk read from wake pipe
constexpr char kWake = '\n';                    //!< Any content is enough to wake up thread

//! Indexes for pipe file descriptors
constexpr int kPipeRead = 0;
constexpr int kPipeWrite = 1;

//! Name requested on session bus (another instance uses it followed by its process identifier)
constexpr char kBusName[] = "org.mpris.MediaPlayer2.spectrum";
constexpr char kInstanceSuffix[] = ".instance";

//! Directory (inside runtime directory, cleared when user logs out) for cover art from song, which
//! is accessible only by the current user
constexpr char kCoverDirectory[] = "spectrum";
constexpr char kCoverPrefix[] = "cover-";
constexpr mode_t kCoverDirectoryMode = S_IRWXU;

constexpr char kObjectPath[] = "/org/mpris/MediaPlayer2";
constexpr char kRootInterface[] = "org.mpris.MediaPlayer2";
constexpr char kPlayerInterface[] = "org.mpris.MediaPlayer2.Player";

constexpr char kIdentity[] = "spectrum";           //!< Name shown by clients
constexpr double kRate = 1.0;                      //!< Playback rate cannot be changed
constexpr char kSignalSeeked[] = "Seeked";         //!< Signal sent when position jumps
constexpr char kErrorFailed[] = "Command failed";  //!< Message for a command not executed

//! URI schemes and MIME types accepted by OpenUri
constexpr std::array<const char*, 3> kUriSchemes{"file", "http", "https"};
constexpr std::array<const char*, 8> kMimeTypes{"audio/mpeg", "audio/flac", "audio/ogg",
                                                "audio/opus", "audio/wav",  "audio/x-wav",
                                                "audio/mp4",  "audio/aac"};

//! Timeout from sd-bus meaning that it has nothing to wait for
constexpr uint64_t kNoTimeout = UINT64_MAX;

/* ********************************************************************************************** */

//! Close file descriptor (when it is opened)
void Close(int& fd) {
  if (fd == kInvalidFd) return;

  close(fd);
  fd = kInvalidFd;
}

/* ********************************************************************************************** */

//! Write to pipe just to wake up thread (nobody waits for it, as it is called by any thread)
void Wake(int fd) {
  while (write(fd, &kWake, sizeof(kWake)) < 0 && errno == EINTR) {
  }
}

/* ********************************************************************************************** */

//! Get time (in milliseconds) to wait for session bus (or -1 to wait without a limit)
int GetPollTimeout(sd_bus* bus) {
  uint64_t deadline = kNoTimeout;
  if (sd_bus_get_timeout(bus, &deadline) < 0 || deadline == kNoTimeout) return -1;

  // Deadline is an absolute time from monotonic clock, in microseconds
  const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch());
  const auto remaining = std::chrono::microseconds{deadline} - now;
  if (remaining.count() <= 0) return 0;

  // Rounded up, otherwise thread would wake up right before deadline
  return static_cast<int>(std::chrono::ceil<std::chrono::milliseconds>(remaining).count());
}

/* ********************************************************************************************** */

//! Append list of texts to reply
template <std::size_t size>
int AppendStrings(sd_bus_message* reply, const std::array<const char*, size>& values) {
  int result = sd_bus_message_open_container(reply, 'a', "s");

  for (const char* value : values) {
    if (result < 0) return result;
    result = sd_bus_message_append(reply, "s", value);
  }

  if (result < 0) return result;
  return sd_bus_message_close_container(reply);
}

/* ********************************************************************************************** */

//! Append song information to reply, as a dictionary
int AppendMetadata(sd_bus_message* reply, const model::PlayerStatus& status,
                   const std::string& cover_url) {
  const std::string track = model::GetMprisTrackId(status);
  const std::string url = model::GetMprisUrl(status);

  int result = sd_bus_message_open_container(reply, 'a', "{sv}");
  if (result < 0) return result;

  result = sd_bus_message_append(reply, "{sv}", "mpris:trackid", "o", track.c_str());

  if (result >= 0 && !status.title.empty()) {
    result = sd_bus_message_append(reply, "{sv}", "xesam:title", "s", status.title.c_str());

    if (result >= 0 && status.duration > 0) {
      result = sd_bus_message_append(reply, "{sv}", "mpris:length", "x",
                                     model::ToMprisTime(status.duration));
    }

    if (result >= 0 && !status.artist.empty()) {
      result = sd_bus_message_append(reply, "{sv}", "xesam:artist", "as", /*count=*/1,
                                     status.artist.c_str());
    }

    // Some clients use it to show a picture for songs from YouTube (as their thumbnail)
    if (result >= 0 && !url.empty())
      result = sd_bus_message_append(reply, "{sv}", "xesam:url", "s", url.c_str());

    if (result >= 0 && !cover_url.empty())
      result = sd_bus_message_append(reply, "{sv}", "mpris:artUrl", "s", cover_url.c_str());
  }

  if (result < 0) return result;
  return sd_bus_message_close_container(reply);
}

}  // namespace

/* ********************************************************************************************** */

/**
 * @brief Callbacks registered on session bus, which have access to the server (received as user
 * data). All of them are called by server thread
 */
struct MprisServer::Callbacks {
  //! Run command and send an empty reply (or an error, if it failed)
  static int Run(sd_bus_message* message, void* userdata, sd_bus_error* error,
                 const model::RemoteRequest& request) {
    auto* server = static_cast<MprisServer*>(userdata);
    LOG("Received MPRIS command=", request);

    if (!server->handler_(request))
      return sd_bus_error_set(error, SD_BUS_ERROR_FAILED, kErrorFailed);

    return sd_bus_reply_method_return(message, "");
  }

  //! Run a command without value
  template <model::RemoteCommand command>
  static int RunCommand(sd_bus_message* message, void* userdata, sd_bus_error* error) {
    return Run(message, userdata, error, command);
  }

  //! Nothing to do, as there is no window to show
  static int Raise(sd_bus_message* message, void*, sd_bus_error*) {
    return sd_bus_reply_method_return(message, "");
  }

  //! Change position by offset (in microseconds)
  static int Seek(sd_bus_message* message, void* userdata, sd_bus_error* error) {
    int64_t offset = 0;
    if (int result = sd_bus_message_read(message, "x", &offset); result < 0) return result;

    // As stated by specification, nothing is done for an offset that cannot be used
    auto request = model::CreateMprisSeek(offset);
    if (!request) return sd_bus_reply_method_return(message, "");

    return Run(message, userdata, error, *request);
  }

  //! Change position of the given song (in microseconds)
  static int SetPosition(sd_bus_message* message, void* userdata, sd_bus_error* error) {
    const char* track = nullptr;
    int64_t position = 0;
    if (int result = sd_bus_message_read(message, "ox", &track, &position); result < 0)
      return result;

    // As stated by specification, nothing is done for another song or a position outside of it
    auto* server = static_cast<MprisServer*>(userdata);
    auto request = model::CreateMprisSetPosition(server->GetStatus(), track, position);
    if (!request) return sd_bus_reply_method_return(message, "");

    return Run(message, userdata, error, *request);
  }

  //! Play the given URI (a local file or HTTP(S) URL)
  static int OpenUri(sd_bus_message* message, void* userdata, sd_bus_error* error) {
    const char* uri = nullptr;
    if (int result = sd_bus_message_read(message, "s", &uri); result < 0) return result;

    auto request = model::CreateMprisOpenUri(uri);
    if (!request) {
      return sd_bus_error_setf(error, SD_BUS_ERROR_INVALID_ARGS, "Unsupported URI \"%s\"", uri);
    }

    return Run(message, userdata, error, *request);
  }

  //! Get property from root interface (none of them change)
  static int GetRootProperty(sd_bus*, const char*, const char*, const char* property,
                             sd_bus_message* reply, void*, sd_bus_error*) {
    const std::string_view name{property};

    if (name == "Identity") return sd_bus_message_append(reply, "s", kIdentity);
    if (name == "CanQuit") return sd_bus_message_append(reply, "b", true);

    if (name == "SupportedUriSchemes") return AppendStrings(reply, kUriSchemes);
    if (name == "SupportedMimeTypes") return AppendStrings(reply, kMimeTypes);

    // CanRaise and HasTrackList
    return sd_bus_message_append(reply, "b", false);
  }

  //! Get property from player interface
  static int GetPlayerProperty(sd_bus*, const char*, const char*, const char* property,
                               sd_bus_message* reply, void* userdata, sd_bus_error*) {
    const auto* server = static_cast<MprisServer*>(userdata);
    const auto status = server->GetStatus();
    const std::string_view name{property};

    if (name == model::kMprisPlaybackStatus) {
      const std::string value{model::GetMprisPlaybackStatus(status.state)};
      return sd_bus_message_append(reply, "s", value.c_str());
    }

    if (name == model::kMprisLoopStatus) {
      const std::string value{model::GetMprisLoopStatus(status.repeat)};
      return sd_bus_message_append(reply, "s", value.c_str());
    }

    if (name == model::kMprisShuffle) return sd_bus_message_append(reply, "b", status.shuffle);
    if (name == model::kMprisMetadata)
      return AppendMetadata(reply, status, server->GetCoverUrl(status));
    if (name == model::kMprisCanSeek)
      return sd_bus_message_append(reply, "b", model::CanMprisSeek(status));

    // Volume level is kept while muted (the same as status sent to other instances)
    if (name == model::kMprisVolume) {
      return sd_bus_message_append(reply, "d", static_cast<double>(status.volume.GetLevel()));
    }

    if (name == "Position")
      return sd_bus_message_append(reply, "x", model::ToMprisTime(status.position));

    if (name == "Rate" || name == "MinimumRate" || name == "MaximumRate")
      return sd_bus_message_append(reply, "d", kRate);

    // CanGoNext, CanGoPrevious, CanPlay, CanPause and CanControl
    return sd_bus_message_append(reply, "b", true);
  }

  //! Change property from player interface (clients are notified once player changes it)
  static int SetPlayerProperty(sd_bus*, const char*, const char*, const char* property,
                               sd_bus_message* value, void* userdata, sd_bus_error* error) {
    auto* server = static_cast<MprisServer*>(userdata);
    const std::string_view name{property};
    std::optional<model::RemoteRequest> request;

    if (name == model::kMprisLoopStatus) {
      const char* text = nullptr;
      if (int result = sd_bus_message_read(value, "s", &text); result < 0) return result;

      auto mode = model::ParseMprisLoopStatus(text);
      if (!mode) {
        return sd_bus_error_setf(error, SD_BUS_ERROR_INVALID_ARGS, "Invalid loop status \"%s\"",
                                 text);
      }

      request = model::RemoteRequest{model::RemoteCommand::ToggleRepeat, *mode};

    } else if (name == model::kMprisShuffle) {
      int enabled = 0;
      if (int result = sd_bus_message_read(value, "b", &enabled); result < 0) return result;

      request = model::RemoteRequest{model::RemoteCommand::ToggleShuffle, enabled != 0};

    } else if (name == model::kMprisVolume) {
      double volume = 0;
      if (int result = sd_bus_message_read(value, "d", &volume); result < 0) return result;

      request = model::CreateMprisSetVolume(volume);
    }

    if (!request) return sd_bus_error_set(error, SD_BUS_ERROR_PROPERTY_READ_ONLY, property);

    LOG("Received MPRIS command=", *request);
    if (!server->handler_(*request))
      return sd_bus_error_set(error, SD_BUS_ERROR_FAILED, kErrorFailed);

    return 0;
  }

  //! Root interface
  static constexpr sd_bus_vtable kRootVtable[] = {
      SD_BUS_VTABLE_START(0),
      SD_BUS_METHOD("Raise", "", "", Raise, 0),
      SD_BUS_METHOD("Quit", "", "", RunCommand<model::RemoteCommand::Quit>, 0),
      SD_BUS_PROPERTY("CanQuit", "b", GetRootProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("CanRaise", "b", GetRootProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("HasTrackList", "b", GetRootProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("Identity", "s", GetRootProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("SupportedUriSchemes", "as", GetRootProperty, 0,
                      SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("SupportedMimeTypes", "as", GetRootProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_VTABLE_END,
  };

  //! Player interface (position changes all the time, so clients must ask for it)
  static constexpr sd_bus_vtable kPlayerVtable[] = {
      SD_BUS_VTABLE_START(0),
      SD_BUS_METHOD("Next", "", "", RunCommand<model::RemoteCommand::SkipToNext>, 0),
      SD_BUS_METHOD("Previous", "", "", RunCommand<model::RemoteCommand::SkipToPrevious>, 0),
      SD_BUS_METHOD("Pause", "", "", RunCommand<model::RemoteCommand::Pause>, 0),
      SD_BUS_METHOD("PlayPause", "", "", RunCommand<model::RemoteCommand::PlayOrPause>, 0),
      SD_BUS_METHOD("Stop", "", "", RunCommand<model::RemoteCommand::Stop>, 0),
      SD_BUS_METHOD("Play", "", "", RunCommand<model::RemoteCommand::Play>, 0),
      SD_BUS_METHOD("Seek", "x", "", Seek, 0),
      SD_BUS_METHOD("SetPosition", "ox", "", SetPosition, 0),
      SD_BUS_METHOD("OpenUri", "s", "", OpenUri, 0),
      SD_BUS_SIGNAL(kSignalSeeked, "x", 0),
      SD_BUS_PROPERTY("PlaybackStatus", "s", GetPlayerProperty, 0,
                      SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
      SD_BUS_WRITABLE_PROPERTY("LoopStatus", "s", GetPlayerProperty, SetPlayerProperty, 0,
                               SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
      SD_BUS_PROPERTY("Rate", "d", GetPlayerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_WRITABLE_PROPERTY("Shuffle", "b", GetPlayerProperty, SetPlayerProperty, 0,
                               SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
      SD_BUS_PROPERTY("Metadata", "a{sv}", GetPlayerProperty, 0,
                      SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
      SD_BUS_WRITABLE_PROPERTY("Volume", "d", GetPlayerProperty, SetPlayerProperty, 0,
                               SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
      SD_BUS_PROPERTY("Position", "x", GetPlayerProperty, 0, 0),
      SD_BUS_PROPERTY("MinimumRate", "d", GetPlayerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("MaximumRate", "d", GetPlayerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("CanGoNext", "b", GetPlayerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("CanGoPrevious", "b", GetPlayerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("CanPlay", "b", GetPlayerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("CanPause", "b", GetPlayerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_PROPERTY("CanSeek", "b", GetPlayerProperty, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
      SD_BUS_PROPERTY("CanControl", "b", GetPlayerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
      SD_BUS_VTABLE_END,
  };
};

/* ********************************************************************************************** */

MprisServer::MprisServer(sd_bus* bus, Handler handler)
    : bus_{bus}, handler_{std::move(handler)}, wake_fd_{kInvalidFd, kInvalidFd} {}

/* ********************************************************************************************** */

std::unique_ptr<MprisServer> MprisServer::Create(Handler handler) {
  sd_bus* bus = nullptr;

  if (int result = sd_bus_open_user(&bus); result < 0) {
    WARN("Cannot connect to D-Bus session bus for MPRIS, error=", std::strerror(-result));
    return nullptr;
  }

  // Simply extend the MprisServer class, as we do not want to expose the default constructor,
  // neither do we want to use std::make_unique explicitly calling operator new()
  struct MakeUniqueEnabler : public MprisServer {
    explicit MakeUniqueEnabler(sd_bus* b, Handler h) : MprisServer(b, std::move(h)) {}
  };

  std::unique_ptr<MprisServer> server =
      std::make_unique<MakeUniqueEnabler>(bus, std::move(handler));

  // Nobody waits to write on pipe, as it may be done by threads that must not be blocked
  if (pipe2(server->wake_fd_.data(), O_CLOEXEC | O_NONBLOCK) != 0) {
    ERROR("Cannot create pipe for MPRIS, error=", std::strerror(errno));
    return nullptr;
  }

  int result = sd_bus_add_object_vtable(bus, nullptr, kObjectPath, kRootInterface,
                                        Callbacks::kRootVtable, server.get());

  if (result >= 0) {
    result = sd_bus_add_object_vtable(bus, nullptr, kObjectPath, kPlayerInterface,
                                      Callbacks::kPlayerVtable, server.get());
  }

  if (result < 0) {
    ERROR("Cannot register MPRIS interfaces, error=", std::strerror(-result));
    return nullptr;
  }

  // As stated by specification, each instance must use a unique name
  std::string name = kBusName;
  result = sd_bus_request_name(bus, name.c_str(), 0);

  if (result == -EEXIST) {
    name += kInstanceSuffix + std::to_string(getpid());
    result = sd_bus_request_name(bus, name.c_str(), 0);
  }

  if (result < 0) {
    ERROR("Cannot request name for MPRIS, error=", std::strerror(-result));
    return nullptr;
  }

  server->thread_ = std::thread(&MprisServer::Loop, server.get());

  INFO("Listening for MPRIS commands on name=", std::quoted(name));
  return server;
}

/* ********************************************************************************************** */

MprisServer::~MprisServer() { Stop(); }

/* ********************************************************************************************** */

void MprisServer::Stop() {
  if (thread_.joinable()) {
    stopping_ = true;
    Wake(wake_fd_[kPipeWrite]);

    thread_.join();
  }

  RemoveCover();

  // Name is released when connection is closed
  if (bus_ != nullptr) bus_ = sd_bus_flush_close_unref(bus_);

  Close(wake_fd_[kPipeRead]);
  Close(wake_fd_[kPipeWrite]);
}

/* ********************************************************************************************** */

void MprisServer::Publish(const model::PlayerStatus& status) {
  {
    std::scoped_lock lock(publish_mutex_);
    published_ = status;

    // Thread was already woken up to send the previous status, so it sends this one instead
    if (pending_) return;
    pending_ = true;
  }

  Wake(wake_fd_[kPipeWrite]);
}

/* ********************************************************************************************** */

model::PlayerStatus MprisServer::GetStatus() const {
  std::scoped_lock lock(publish_mutex_);
  return published_;
}

/* ********************************************************************************************** */

void MprisServer::UpdateCover(const model::PlayerStatus& status) {
  RemoveCover();
  if (!status.cover || status.cover->empty()) return;

  // Without runtime directory, there is no place to keep it only while song is playing
  const char* runtime = std::getenv("XDG_RUNTIME_DIR");
  if (!runtime || !std::filesystem::path{runtime}.is_absolute()) return;

  // It is fine to fail here when directory already exists (writing to file is checked next)
  const std::filesystem::path directory = std::filesystem::path{runtime} / kCoverDirectory;
  mkdir(directory.c_str(), kCoverDirectoryMode);

  // Each song uses a new name (also unique per instance), otherwise clients would keep showing the
  // picture they already loaded
  const std::filesystem::path path =
      directory /
      (kCoverPrefix + std::to_string(getpid()) + "-" + std::to_string(status.song_count) +
       std::string{model::GetMprisImageExtension(*status.cover)});

  std::ofstream file{path, std::ios::binary | std::ios::trunc};
  file.write(status.cover->data(), static_cast<std::streamsize>(status.cover->size()));
  file.close();

  if (!file) {
    ERROR("Cannot write cover art to file=", path);

    std::error_code error;
    std::filesystem::remove(path, error);
    return;
  }

  cover_path_ = path;
  cover_song_ = status.song_count;
}

/* ********************************************************************************************** */

void MprisServer::RemoveCover() {
  if (cover_path_.empty()) return;

  std::error_code error;
  std::filesystem::remove(cover_path_, error);
  cover_path_.clear();
}

/* ********************************************************************************************** */

std::string MprisServer::GetCoverUrl(const model::PlayerStatus& status) const {
  // Song may have changed after file was written (it is written only when changes are sent)
  if (cover_path_.empty() || cover_song_ != status.song_count) return {};

  return model::ToMprisFileUrl(cover_path_.string());
}

/* ********************************************************************************************** */

void MprisServer::Loop() {
  util::Logger::SetThreadName("mpris");

  // Position of each file descriptor in the list to wait for
  enum Polled { Bus, Wake, Total };

  while (true) {
    // Handle everything received (and send everything queued) before waiting again
    int result = sd_bus_process(bus_, nullptr);

    if (result < 0) {
      ERROR("Cannot handle MPRIS requests, error=", std::strerror(-result));
      break;
    }

    if (result > 0 || SendPublished()) continue;

    std::array<pollfd, Polled::Total> fds;
    fds[Polled::Bus] = pollfd{.fd = sd_bus_get_fd(bus_),
                              .events = static_cast<short>(sd_bus_get_events(bus_)),
                              .revents = 0};
    fds[Polled::Wake] = pollfd{.fd = wake_fd_[kPipeRead], .events = POLLIN, .revents = 0};

    if (poll(fds.data(), fds.size(), GetPollTimeout(bus_)) < 0) {
      if (errno == EINTR) continue;

      ERROR("Cannot wait for MPRIS requests, error=", std::strerror(errno));
      break;
    }

    if (fds[Polled::Wake].revents != 0) {
      // Received command to stop
      if (stopping_) break;

      // Otherwise there is status to send, so discard what was written just to wake up thread
      std::array<char, kDiscardBufferSize> discarded;
      while (read(wake_fd_[kPipeRead], discarded.data(), discarded.size()) < 0 && errno == EINTR) {
      }
    }
  }
}

/* ********************************************************************************************** */

bool MprisServer::SendPublished() {
  model::PlayerStatus status;

  {
    std::scoped_lock lock(publish_mutex_);
    if (!pending_) return false;

    status = published_;
    pending_ = false;
  }

  // Written before notifying clients about the new song, so they receive both at the same time
  if (status.song_count != sent_.song_count || status.cover != sent_.cover) UpdateCover(status);

  auto changes = model::GetMprisChanges(sent_, status);
  sent_ = status;

  if (!changes.properties.empty()) {
    // Values are read again from the last status published, when message is created
    std::vector<char*> names;
    for (auto& property : changes.properties) names.push_back(property.data());
    names.push_back(nullptr);

    if (int result =
            sd_bus_emit_properties_changed_strv(bus_, kObjectPath, kPlayerInterface, names.data());
        result < 0) {
      ERROR("Cannot notify MPRIS properties changed, error=", std::strerror(-result));
    }
  }

  if (changes.seeked) {
    if (int result = sd_bus_emit_signal(bus_, kObjectPath, kPlayerInterface, kSignalSeeked, "x",
                                        model::ToMprisTime(status.position));
        result < 0) {
      ERROR("Cannot notify MPRIS position changed, error=", std::strerror(-result));
    }
  }

  return true;
}

}  // namespace util
