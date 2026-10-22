#include "middleware/remote_client.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <variant>

#include "middleware/remote_playlist.h"
#include "model/player_status.h"
#include "model/remote_command.h"
#include "util/remote.h"

namespace middleware {

namespace {

/**
 * @brief Print status from the running instance every time it changes, until that instance exits
 *
 * @param path Socket path used by the running instance
 * @param format Text with fields to print status (if empty, it is printed as received, in JSON)
 * @param out Where to print status
 * @param err Where to print errors
 * @return EXIT_SUCCESS if status was received from the running instance, otherwise EXIT_FAILURE
 */
int PrintRemoteStatusUpdates(const std::string& path, const std::optional<std::string>& format,
                             std::ostream& out, std::ostream& err) {
  bool failed = false;
  std::optional<std::string> printed;

  auto print = [&format, &out, &err, &failed, &printed](const std::string& update) {
    auto status = model::format_status(update, format);

    // Anything other than status is an error message
    if (!status) {
      err << "spectrum: " << update << "\n";
      failed = true;
      return false;
    }

    // Nothing new to print when the change was on a field not used by text
    if (status == printed) return true;
    printed = status;

    // Whoever is reading it (e.g. a status bar) must receive each update right away
    out << *status << std::endl;
    return out.good();
  };

  bool sent = util::ReceiveRemoteUpdates(path, std::string{model::kRemoteSubscribeQuery}, print);

  if (!sent) {
    err << "spectrum: there is no running instance to control\n";
    return EXIT_FAILURE;
  }

  return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

}  // namespace

/* ********************************************************************************************** */

int SendRemoteCommand(const std::string& path, const std::string& command,
                      const std::optional<std::string>& format, std::ostream& out,
                      std::ostream& err) {
  if (command == model::kRemoteSubscribeQuery)
    return PrintRemoteStatusUpdates(path, format, out, err);

  const bool is_query = command == model::kRemoteStatusQuery;
  std::string request = command;

  if (!is_query) {
    std::string error;
    auto parsed = model::ParseRemoteRequest(command, error);

    if (!parsed) {
      err << "spectrum: " << error;

      // Command exists when error is about its value, so there is no reason to list all of them
      if (!model::ParseRemoteCommand(command.substr(0, command.find(' ')))) {
        err << " (available: " << model::GetRemoteCommandNames() << ")";
      }

      err << "\n";
      return EXIT_FAILURE;
    }

    // Running instance has another working directory, so it must receive the full path. Anything
    // that does not exist here is sent as it is (e.g. name of a playlist)
    if (auto* target = std::get_if<std::string>(&parsed->value); target && !IsRemoteUrl(*target)) {
      std::error_code failure;

      if (auto file = std::filesystem::absolute(*target, failure);
          !failure && std::filesystem::exists(file, failure)) {
        *target = file.lexically_normal().string();
      }
    }

    // Send it in a single format, no matter how it was written
    std::ostringstream text;
    text << *parsed;
    request = text.str();
  }

  auto reply = util::SendRemoteRequest(path, request);

  if (!reply) {
    err << "spectrum: there is no running instance to control\n";
    return EXIT_FAILURE;
  }

  if (is_query) {
    // Anything other than status is an error message
    auto status = model::format_status(*reply, format);

    if (!status) {
      err << "spectrum: " << *reply << "\n";
      return EXIT_FAILURE;
    }

    out << *status << "\n";
    return EXIT_SUCCESS;
  }

  if (*reply != model::kRemoteReplyOk) {
    err << "spectrum: " << *reply << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

}  // namespace middleware
