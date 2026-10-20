/**
 * \file
 * \brief  Server to control this application through MPRIS (D-Bus), e.g. by media keys
 */

#ifndef INCLUDE_UTIL_MPRIS_H_
#define INCLUDE_UTIL_MPRIS_H_

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "model/player_status.h"
#include "model/remote_command.h"

//! Forward declaration (from sd-bus)
struct sd_bus;

namespace util {

/**
 * @brief Register this instance as a media player on the D-Bus session bus (following MPRIS
 * specification), so it can be controlled by media keys, desktop widgets and tools like playerctl
 */
class MprisServer {
 public:
  //! Run command received (return false when it could not be executed)
  using Handler = std::function<bool(const model::RemoteRequest&)>;

 private:
  /**
   * @brief Construct a new MprisServer object
   * @param bus Connection to session bus
   * @param handler Callback to run each command
   */
  MprisServer(sd_bus* bus, Handler handler);

 public:
  /**
   * @brief Factory method: connect to session bus, register as a media player and handle requests
   * from it using a new thread
   * @param handler Callback to run each command
   * @return MprisServer unique instance (or null, if there is no session bus or registration
   * failed)
   */
  static std::unique_ptr<MprisServer> Create(Handler handler);

  /**
   * @brief Destroy the MprisServer object
   */
  ~MprisServer();

  //! Remove these
  MprisServer(const MprisServer& other) = delete;             // copy constructor
  MprisServer(MprisServer&& other) = delete;                  // move constructor
  MprisServer& operator=(const MprisServer& other) = delete;  // copy assignment
  MprisServer& operator=(MprisServer&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Stop handling requests and disconnect from session bus
   */
  void Stop();

  /**
   * @brief Set player status, notifying what has changed to clients (may be called from any thread,
   * as it is sent later by server thread). When published faster than it is sent, only the last
   * one is sent
   * @param status Player status
   */
  void Publish(const model::PlayerStatus& status);

  /* ******************************************************************************************** */
 private:
  //! Callbacks registered on session bus (defined only in source file)
  struct Callbacks;

  //! Main loop for server thread
  void Loop();

  //! Notify clients about changes from the last status sent (return false if nothing was pending)
  bool SendPublished();

  //! Get the last status published
  model::PlayerStatus GetStatus() const;

  //! Write cover art from song to a new file, removing the one from previous song
  void UpdateCover(const model::PlayerStatus& status);

  //! Remove file with cover art (if any)
  void RemoveCover();

  //! Get URL for cover art from song (or empty, if it was not written to file)
  std::string GetCoverUrl(const model::PlayerStatus& status) const;

  /* ******************************************************************************************** */
  //! Variables

  sd_bus* bus_;                 //!< Connection to session bus
  Handler handler_;             //!< Callback to run each command
  std::array<int, 2> wake_fd_;  //!< Pipe used to wake up thread, to stop it or send status
  std::thread thread_;          //!< Thread handling requests from session bus

  std::atomic<bool> stopping_ = false;  //!< Thread was woken up to stop
  model::PlayerStatus sent_;            //!< Last status sent to clients (used only by thread)

  std::filesystem::path cover_path_;  //!< File with cover art from song (used only by thread)
  uint32_t cover_song_ = 0;           //!< Song (from its count) whose cover art is in that file

  mutable std::mutex publish_mutex_;  //!< Control access to published status
  model::PlayerStatus published_;     //!< Last status published
  bool pending_ = false;              //!< Last status published was not sent to clients yet
};

}  // namespace util
#endif  // INCLUDE_UTIL_MPRIS_H_
