/**
 * \file
 * \brief  Remote control of a running instance, using a Unix domain socket
 */

#ifndef INCLUDE_UTIL_REMOTE_H_
#define INCLUDE_UTIL_REMOTE_H_

#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace util {

//! Maximum time to wait for the other side (to send its request or its reply)
inline constexpr std::chrono::milliseconds kRemoteTimeout{1000};

/**
 * @brief Get full path to socket used for remote control, which is "$XDG_RUNTIME_DIR/spectrum.sock"
 * or, when that is not set, "/tmp/spectrum-<uid>.sock"
 * @return String containing socket path
 */
std::string GetRemoteSocketPath();

/**
 * @brief Send a single request to the running instance and wait for its reply
 * @param path Socket path
 * @param request Request (a single line of text)
 * @param timeout Maximum time to wait for reply
 * @return Reply from running instance (or nothing, if there is no instance listening on socket)
 */
std::optional<std::string> SendRemoteRequest(const std::string& path, const std::string& request,
                                             std::chrono::milliseconds timeout = kRemoteTimeout);

/**
 * @brief Listen on a socket for requests sent by other instances (one line of text per connection)
 */
class RemoteServer {
 public:
  //! Called from server thread for each request received, the returned value is sent as reply
  using Handler = std::function<std::string(const std::string&)>;

 private:
  /**
   * @brief Construct a new RemoteServer object
   * @param path Socket path
   * @param socket_fd Socket already listening for connections
   * @param handler Callback to handle each request
   */
  RemoteServer(const std::string& path, int socket_fd, Handler handler);

 public:
  /**
   * @brief Factory method: Create socket and start listening on it using a new thread
   * @param path Socket path
   * @param handler Callback to handle each request
   * @return RemoteServer unique instance (or null, if socket is already used by another instance
   * or could not be created)
   */
  static std::unique_ptr<RemoteServer> Create(const std::string& path, Handler handler);

  /**
   * @brief Destroy the RemoteServer object
   */
  ~RemoteServer();

  //! Remove these
  RemoteServer(const RemoteServer& other) = delete;             // copy constructor
  RemoteServer(RemoteServer&& other) = delete;                  // move constructor
  RemoteServer& operator=(const RemoteServer& other) = delete;  // copy assignment
  RemoteServer& operator=(RemoteServer&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Stop listening and remove socket
   */
  void Stop();

  /* ******************************************************************************************** */
 private:
  //! Main loop for server thread
  void Loop();

  //! Read request from a new connection, handle it and send reply
  void HandleConnection();

  /* ******************************************************************************************** */
  //! Variables

  std::string path_;            //!< Socket path
  int socket_fd_;               //!< Socket listening for connections
  std::array<int, 2> wake_fd_;  //!< Pipe used to wake up thread, to stop it
  Handler handler_;             //!< Callback to handle each request
  std::thread thread_;          //!< Thread listening on socket
};

}  // namespace util
#endif  // INCLUDE_UTIL_REMOTE_H_
