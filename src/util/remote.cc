#include "util/remote.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <utility>

#include "util/logger.h"

namespace util {

namespace {

constexpr int kInvalidFd = -1;                //!< File descriptor not opened
constexpr int kMaxPendingConnections = 4;     //!< Connections waiting to be accepted
constexpr std::size_t kMaxMessageSize = 256;  //!< Maximum size for a request or a reply
constexpr char kMessageEnd = '\n';            //!< Every request and reply is a single line

//! Only the current user may access the socket and (when created by us) its directory
constexpr mode_t kSocketMode = S_IRUSR | S_IWUSR;
constexpr mode_t kDirectoryMode = S_IRWXU;
constexpr mode_t kAccessByOthers = S_IRWXG | S_IRWXO;

//! Indexes for pipe file descriptors
constexpr int kPipeRead = 0;
constexpr int kPipeWrite = 1;

/* ********************************************************************************************** */

//! Close file descriptor (when it is opened)
void Close(int& fd) {
  if (fd == kInvalidFd) return;

  close(fd);
  fd = kInvalidFd;
}

/* ********************************************************************************************** */

//! Get directory where socket is located
std::string GetDirectory(const std::string& socket_path) {
  return std::filesystem::path{socket_path}.parent_path().string();
}

/* ********************************************************************************************** */

/**
 * @brief Check if it is a real directory (not a link to one) that belongs to the current user and
 * nobody else can access. Otherwise, another user could replace the socket to receive commands
 */
bool IsPrivateDirectory(const std::string& directory) {
  struct stat info;
  if (lstat(directory.c_str(), &info) != 0) return false;

  return S_ISDIR(info.st_mode) && info.st_uid == geteuid() && (info.st_mode & kAccessByOthers) == 0;
}

/* ********************************************************************************************** */

//! Fill socket address with path (return false when path does not fit)
bool FillAddress(const std::string& path, sockaddr_un& address) {
  if (path.empty() || path.size() >= sizeof(address.sun_path)) return false;

  std::memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, path.c_str(), path.size());

  return true;
}

/* ********************************************************************************************** */

//! Create a socket connected to the instance listening on path
int Connect(const std::string& path) {
  sockaddr_un address;
  if (!FillAddress(path, address)) return kInvalidFd;

  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd == kInvalidFd) return kInvalidFd;

  if (connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) Close(fd);

  return fd;
}

/* ********************************************************************************************** */

//! Send a single line of text
bool SendLine(int fd, const std::string& content) {
  const std::string message = content + kMessageEnd;
  std::size_t sent = 0;

  while (sent < message.size()) {
    // Do not raise a signal when the other side has already closed its connection
    ssize_t count = send(fd, message.data() + sent, message.size() - sent, MSG_NOSIGNAL);

    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return false;

    sent += static_cast<std::size_t>(count);
  }

  return true;
}

/* ********************************************************************************************** */

//! Receive a single line of text (or nothing, if the other side took too long or closed before it)
std::optional<std::string> ReceiveLine(int fd, std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::string line;

  while (line.size() < kMaxMessageSize) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0) return std::nullopt;

    pollfd pfd{.fd = fd, .events = POLLIN, .revents = 0};
    int ready = poll(&pfd, 1, static_cast<int>(remaining.count()));

    if (ready < 0 && errno == EINTR) continue;
    if (ready <= 0) return std::nullopt;

    std::array<char, kMaxMessageSize> buffer;
    ssize_t count = recv(fd, buffer.data(), buffer.size(), 0);

    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return std::nullopt;

    line.append(buffer.data(), static_cast<std::size_t>(count));

    if (auto end = line.find(kMessageEnd); end != std::string::npos) {
      line.erase(end);
      return line;
    }
  }

  return std::nullopt;
}

}  // namespace

/* ********************************************************************************************** */

std::string GetRemoteSocketPath() {
  // Relative path is not valid for this variable, so it is ignored (as stated by XDG specification)
  if (const char* runtime = std::getenv("XDG_RUNTIME_DIR");
      runtime && std::filesystem::path{runtime}.is_absolute()) {
    return (std::filesystem::path{runtime} / "spectrum.sock").string();
  }

  // As this directory is shared by all users, socket is created inside a private one
  return "/tmp/spectrum-" + std::to_string(geteuid()) + "/spectrum.sock";
}

/* ********************************************************************************************** */

std::optional<std::string> SendRemoteRequest(const std::string& path, const std::string& request,
                                             std::chrono::milliseconds timeout) {
  // Do not trust a socket that could have been created by someone else
  if (!IsPrivateDirectory(GetDirectory(path))) return std::nullopt;

  int fd = Connect(path);
  if (fd == kInvalidFd) return std::nullopt;

  std::optional<std::string> reply;
  if (SendLine(fd, request)) reply = ReceiveLine(fd, timeout);

  Close(fd);
  return reply;
}

/* ********************************************************************************************** */

RemoteServer::RemoteServer(const std::string& path, int socket_fd, Handler handler)
    : path_{path},
      socket_fd_{socket_fd},
      wake_fd_{kInvalidFd, kInvalidFd},
      handler_{std::move(handler)} {}

/* ********************************************************************************************** */

std::unique_ptr<RemoteServer> RemoteServer::Create(const std::string& path, Handler handler) {
  sockaddr_un address;
  if (!FillAddress(path, address)) {
    ERROR("Invalid path for remote control socket=", std::quoted(path));
    return nullptr;
  }

  // Create directory when it does not exist yet (it is fine to fail here, as it is checked next)
  const std::string directory = GetDirectory(path);
  mkdir(directory.c_str(), kDirectoryMode);

  if (!IsPrivateDirectory(directory)) {
    ERROR(
        "Directory for remote control socket must belong to (and be accessible only by) the "
        "current user, path=",
        std::quoted(directory));
    return nullptr;
  }

  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd == kInvalidFd) {
    ERROR("Cannot create remote control socket, error=", std::strerror(errno));
    return nullptr;
  }

  const auto* generic = reinterpret_cast<const sockaddr*>(&address);
  int result = bind(fd, generic, sizeof(address));

  if (result != 0 && errno == EADDRINUSE) {
    // Socket file exists: either it is used by another instance, or it was left by one that did
    // not exit properly (in this case, nobody is listening on it and it can be replaced)
    if (int other = Connect(path); other != kInvalidFd) {
      Close(other);
      Close(fd);

      WARN("Remote control socket is used by another instance, path=", std::quoted(path));
      return nullptr;
    }

    unlink(path.c_str());
    result = bind(fd, generic, sizeof(address));
  }

  // Nobody is able to connect before listen(), so there is no window with default permissions
  if (result == 0) result = chmod(path.c_str(), kSocketMode);

  if (result != 0 || listen(fd, kMaxPendingConnections) != 0) {
    ERROR("Cannot listen on remote control socket=", std::quoted(path),
          ", error=", std::strerror(errno));
    Close(fd);
    return nullptr;
  }

  // Simply extend the RemoteServer class, as we do not want to expose the default constructor,
  // neither do we want to use std::make_unique explicitly calling operator new()
  struct MakeUniqueEnabler : public RemoteServer {
    explicit MakeUniqueEnabler(const std::string& p, int f, Handler h)
        : RemoteServer(p, f, std::move(h)) {}
  };

  std::unique_ptr<RemoteServer> server =
      std::make_unique<MakeUniqueEnabler>(path, fd, std::move(handler));

  if (pipe2(server->wake_fd_.data(), O_CLOEXEC) != 0) {
    ERROR("Cannot create pipe for remote control, error=", std::strerror(errno));
    return nullptr;
  }

  server->thread_ = std::thread(&RemoteServer::Loop, server.get());

  INFO("Listening for remote commands on socket=", std::quoted(path));
  return server;
}

/* ********************************************************************************************** */

RemoteServer::~RemoteServer() { Stop(); }

/* ********************************************************************************************** */

void RemoteServer::Stop() {
  if (thread_.joinable()) {
    // Any content is enough to wake up thread
    const char wake = kMessageEnd;
    while (write(wake_fd_[kPipeWrite], &wake, sizeof(wake)) < 0 && errno == EINTR) {
    }

    thread_.join();
  }

  // Socket file is removed only by its owner
  if (socket_fd_ != kInvalidFd) unlink(path_.c_str());

  Close(socket_fd_);
  Close(wake_fd_[kPipeRead]);
  Close(wake_fd_[kPipeWrite]);
}

/* ********************************************************************************************** */

void RemoteServer::Loop() {
  util::Logger::SetThreadName("remote");

  // Position of each file descriptor in the list to wait for
  enum Polled { Socket, Wake, Total };

  while (true) {
    std::array<pollfd, Polled::Total> fds{};
    fds[Polled::Socket] = pollfd{.fd = socket_fd_, .events = POLLIN, .revents = 0};
    fds[Polled::Wake] = pollfd{.fd = wake_fd_[kPipeRead], .events = POLLIN, .revents = 0};

    if (poll(fds.data(), fds.size(), /*timeout=*/-1) < 0) {
      if (errno == EINTR) continue;

      ERROR("Cannot wait for remote commands, error=", std::strerror(errno));
      break;
    }

    // Received command to stop
    if (fds[Polled::Wake].revents != 0) break;

    if (fds[Polled::Socket].revents != 0) HandleConnection();
  }
}

/* ********************************************************************************************** */

void RemoteServer::HandleConnection() {
  int client = accept4(socket_fd_, nullptr, nullptr, SOCK_CLOEXEC);
  if (client == kInvalidFd) return;

  if (auto request = ReceiveLine(client, kRemoteTimeout); request) {
    INFO("Received remote request=", std::quoted(*request));
    SendLine(client, handler_(*request));
  }

  Close(client);
}

}  // namespace util
