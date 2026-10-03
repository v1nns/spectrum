#include "util/process.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <system_error>

extern char** environ;

namespace util {

namespace {

//! Read everything available from file descriptor into output (return false on end of file)
bool ReadFrom(int fd, std::string& output) {
  std::array<char, 4096> buffer;
  ssize_t count = read(fd, buffer.data(), buffer.size());

  if (count > 0) {
    output.append(buffer.data(), static_cast<std::size_t>(count));
    return true;
  }

  // Interrupted by a signal, try again later
  return count < 0 && errno == EINTR;
}

}  // namespace

/* ********************************************************************************************** */

std::optional<std::filesystem::path> FindExecutable(const std::string& name) {
  const char* path = std::getenv("PATH");
  if (!path || name.empty()) return std::nullopt;

  std::istringstream dirs{path};
  std::string dir;

  while (std::getline(dirs, dir, ':')) {
    if (dir.empty()) continue;

    std::filesystem::path candidate = std::filesystem::path{dir} / name;
    std::error_code error;

    if (std::filesystem::is_regular_file(candidate, error) &&
        access(candidate.c_str(), X_OK) == 0) {
      return candidate;
    }
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

std::optional<ProcessResult> RunProcess(const std::vector<std::string>& args,
                                        std::chrono::milliseconds timeout,
                                        const std::atomic<bool>* cancel) {
  // While waiting for program, check from time to time if it was canceled
  static constexpr std::chrono::milliseconds kCancelCheckInterval{100};

  if (args.empty()) return std::nullopt;

  // Pipes to read standard output and error from program
  int out_pipe[2];
  int err_pipe[2];

  if (pipe2(out_pipe, O_CLOEXEC) != 0) return std::nullopt;
  if (pipe2(err_pipe, O_CLOEXEC) != 0) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    return std::nullopt;
  }

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_adddup2(&actions, out_pipe[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, err_pipe[1], STDERR_FILENO);

  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);

  pid_t pid = -1;
  int spawned = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);

  // Only program writes to these ends of pipes
  close(out_pipe[1]);
  close(err_pipe[1]);

  if (spawned != 0) {
    close(out_pipe[0]);
    close(err_pipe[0]);
    return std::nullopt;
  }

  ProcessResult result;
  std::array<pollfd, 2> fds{{{out_pipe[0], POLLIN, 0}, {err_pipe[0], POLLIN, 0}}};
  std::array<std::string*, 2> outputs{&result.output, &result.error};
  int open_fds = static_cast<int>(fds.size());

  const auto deadline = std::chrono::steady_clock::now() + timeout;

  // Read everything from program until both pipes are closed (or until it takes too long)
  while (open_fds > 0) {
    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());

    if (remaining.count() <= 0) {
      result.timed_out = true;
      kill(pid, SIGKILL);
      break;
    }

    if (cancel && *cancel) {
      result.canceled = true;
      kill(pid, SIGKILL);
      break;
    }

    if (cancel) remaining = std::min(remaining, kCancelCheckInterval);

    if (poll(fds.data(), fds.size(), static_cast<int>(remaining.count())) < 0) {
      if (errno == EINTR) continue;
      kill(pid, SIGKILL);
      break;
    }

    for (std::size_t i = 0; i < fds.size(); i++) {
      if (fds[i].fd < 0 || fds[i].revents == 0) continue;

      if (!ReadFrom(fds[i].fd, *outputs[i])) {
        close(fds[i].fd);
        fds[i].fd = -1;
        open_fds--;
      }
    }
  }

  for (const auto& fd : fds) {
    if (fd.fd >= 0) close(fd.fd);
  }

  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }

  if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);

  return result;
}

}  // namespace util
