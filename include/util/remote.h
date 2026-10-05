/**
 * \file
 * \brief  Remote control of a running instance, using a Unix domain socket
 */

#ifndef INCLUDE_UTIL_REMOTE_H_
#define INCLUDE_UTIL_REMOTE_H_

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace util {

//! Maximum time to wait for the other side (to send its request or its reply)
inline constexpr std::chrono::milliseconds kRemoteTimeout{1000};

//! Maximum size for a request, and for a reply (which is bigger, as it may contain player status)
inline constexpr std::size_t kMaxRemoteRequestSize = 256;
inline constexpr std::size_t kMaxRemoteReplySize = 4096;

/**
 * @brief Get full path to socket used for remote control, which is "$XDG_RUNTIME_DIR/spectrum.sock"
 * or, when that is not set, "/tmp/spectrum-<uid>/spectrum.sock"
 * @return String containing socket path
 */
std::string GetRemoteSocketPath();

/**
 * @brief Send a single request to the running instance and wait for its reply
 * @param path Socket path (its directory must be accessible only by the current user, otherwise
 * request is not sent, as someone else could be listening on it)
 * @param request Request (a single line of text)
 * @param timeout Maximum time to wait for reply
 * @return Reply from running instance (or nothing, if there is no instance listening on socket)
 */
std::optional<std::string> SendRemoteRequest(const std::string& path, const std::string& request,
                                             std::chrono::milliseconds timeout = kRemoteTimeout);

/**
 * @brief Send a single request to the running instance and keep receiving everything published by
 * it (one line of text each time), until it closes the connection
 * @param path Socket path (its directory must be accessible only by the current user, otherwise
 * request is not sent, as someone else could be listening on it)
 * @param request Request (a single line of text)
 * @param on_update Called for each line received (return false to stop receiving)
 * @return true if request was sent (or false, if there is no instance listening on socket)
 */
bool ReceiveRemoteUpdates(const std::string& path, const std::string& request,
                          const std::function<bool(const std::string&)>& on_update);

/**
 * @brief Listen on a socket for requests sent by other instances (one line of text per connection).
 * A connection may also be kept open to receive everything published by this instance
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
   * @param subscription Request to keep connection open
   */
  RemoteServer(const std::string& path, int socket_fd, Handler handler,
               const std::string& subscription);

 public:
  /**
   * @brief Factory method: Create socket and start listening on it using a new thread
   * @param path Socket path (its directory is created when it does not exist yet, and it must be
   * accessible only by the current user)
   * @param handler Callback to handle each request
   * @param subscription Request that is not sent to handler: instead, its connection is kept open
   * to receive every content published (if empty, there is no such request)
   * @return RemoteServer unique instance (or null, if socket is already used by another instance
   * or could not be created in a safe way)
   */
  static std::unique_ptr<RemoteServer> Create(const std::string& path, Handler handler,
                                              const std::string& subscription = "");

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

  /**
   * @brief Send content to all subscribers, when it is different from the last one published (may
   * be called from any thread, as it is sent later by server thread). A new subscriber receives
   * the last content right away. When published faster than it is sent, only the last one is sent
   * @param content Content (a single line of text)
   */
  void Publish(const std::string& content);

  /* ******************************************************************************************** */
 private:
  //! Main loop for server thread
  void Loop();

  //! Read request from a new connection, handle it and send reply
  void HandleConnection();

  //! Keep connection open to receive every content published
  void AddSubscriber(int client);

  //! Close connection from subscriber
  void RemoveSubscriber(int client);

  //! Send content to all subscribers, in case of having one not sent yet
  void SendPublished();

  /* ******************************************************************************************** */
  //! Variables

  std::string path_;            //!< Socket path
  int socket_fd_;               //!< Socket listening for connections
  std::array<int, 2> wake_fd_;  //!< Pipe used to wake up thread, to stop it
  Handler handler_;             //!< Callback to handle each request
  std::string subscription_;    //!< Request to keep connection open
  std::thread thread_;          //!< Thread listening on socket

  std::atomic<bool> stopping_ = false;  //!< Thread was woken up to stop
  std::vector<int> subscribers_;        //!< Connections kept open (used only by server thread)

  std::mutex publish_mutex_;  //!< Control access to published content
  std::string published_;     //!< Last content published
  bool pending_ = false;      //!< Last content published was not sent to subscribers yet
};

}  // namespace util
#endif  // INCLUDE_UTIL_REMOTE_H_
