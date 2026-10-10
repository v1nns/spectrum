/**
 * \file
 * \brief Main function
 */
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

#include "audio/player.h"
#include "ftxui/component/loop.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/screen/terminal.hpp"
#include "middleware/media_controller.h"
#include "middleware/remote_playlist.h"
#include "model/player_status.h"
#include "model/remote_command.h"
#include "model/settings.h"
#include "util/arg_parser.h"
#include "util/file_handler.h"
#include "util/logger.h"
#include "util/remote.h"
#include "view/base/terminal.h"

/**
 * @brief A structure containing all available options to configure using command-line arguments
 */
struct Settings {
  std::string log_path = "";     //!< Path to log file (empty if logging is disabled)
  std::string initial_dir = "";  //!< Initial directory to list in "files" block
  bool verbose_logging = false;  //!< Enable verbose log messages

  //! Command to send to the running instance (when filled, a new instance is not started)
  std::optional<std::string> remote_command;

  //! Text with fields to print player status (when empty, it is printed as JSON)
  std::optional<std::string> remote_format;
};

//! Reply sent to remote instance when its command was accepted
static constexpr char kRemoteReplyOk[] = "ok";

/**
 * @brief Command-line argument parsing
 *
 * @param argc Size of array
 * @param argv Array of arguments
 * @param options Configuration options parsed from command-line
 * @return true if parsed successfully, otherwise false
 */
bool parse(int argc, char** argv, Settings& options) {
  using util::Argument;
  using util::ExpectedArguments;
  using util::ParsedArguments;
  using util::Parser;

  try {
    // Create arguments expectation
    auto expected_args = ExpectedArguments{
        Argument{
            .name = "log",
            .choices = {"-l", "--log"},
            .description = "Log to specified path (default: ~/.cache/spectrum/spectrum.log)",
        },
        Argument{
            .name = "directory",
            .choices = {"-d", "--directory"},
            .description = "Initialize listing files from the given directory path",
        },
        Argument{
            .name = "verbose",
            .choices = {"-v", "--verbose"},
            .description = "Enable verbose logging messages",
            .is_empty = true,
        },
        Argument{
            .name = "remote",
            .choices = {"-r", "--remote"},
            .description =
                "Send command to the running instance (" + model::GetRemoteCommandNames() + ")",
            .is_multiple = true,
        },
        Argument{
            .name = "format",
            .choices = {"-f", "--format"},
            .description = "Print status from the running instance using this text instead of "
                           "JSON (e.g. \"{artist} - {title}\")",
        },
    };

    // Configure argument parser and run to get parsed arguments
    Parser arg_parser = util::ArgumentParser::Configure(expected_args);
    ParsedArguments parsed_args = arg_parser->Parse(argc, argv);

    // Command is handled by the running instance, so nothing else is used (not even logging)
    if (auto& remote = parsed_args["remote"]; remote) {
      options.remote_command = remote->get_string();
      if (auto& format = parsed_args["format"]; format)
        options.remote_format = format->get_string();

      return true;
    }

    // Check if contains filepath for logging (otherwise, use default path)
    if (auto& logging_path = parsed_args["log"]; logging_path) {
      options.log_path = logging_path->get_string();
    } else if (util::FileHandler file_handler; !file_handler.GetHome().empty()) {
      options.log_path = file_handler.GetLogPath();

      std::error_code error;
      std::filesystem::create_directories(std::filesystem::path{options.log_path}.parent_path(),
                                          error);
    }

    // Enable logging (log file is renamed when it gets too big, keeping only the previous one)
    if (!options.log_path.empty()) util::Logger::GetInstance().Configure(options.log_path);

    // Check if contains flag for verbose logging
    if (auto& verbose = parsed_args["verbose"]; verbose) {
      options.verbose_logging = verbose->get_bool();
    }

    // Detailed steps are only logged with verbose logging
    util::Logger::GetInstance().SetLevel(options.verbose_logging ? util::LogLevel::Debug
                                                                 : util::LogLevel::Info);

    // Check if contains dirpath for initial file listing
    if (auto& initial_path = parsed_args["directory"]; initial_path) {
      options.initial_dir = initial_path->get_string();
    }

  } catch (util::parsing_error&) {
    // Got some error while trying to parse, or even received help as argument
    // Just let ArgumentParser handle it and exit application
    return false;
  }

  return true;
}

/* ********************************************************************************************** */

/**
 * @brief Print status from the running instance every time it changes, until that instance exits
 *
 * @param format Text with fields to print status (if empty, it is printed as received, in JSON)
 * @return EXIT_SUCCESS if status was received from the running instance, otherwise EXIT_FAILURE
 */
int print_remote_status_updates(const std::optional<std::string>& format) {
  bool failed = false;
  std::optional<std::string> printed;

  auto print = [&format, &failed, &printed](const std::string& update) {
    auto status = model::format_status(update, format);

    // Anything other than status is an error message
    if (!status) {
      std::cerr << "spectrum: " << update << "\n";
      failed = true;
      return false;
    }

    // Nothing new to print when the change was on a field not used by text
    if (status == printed) return true;
    printed = status;

    // Whoever is reading it (e.g. a status bar) must receive each update right away
    std::cout << *status << std::endl;
    return std::cout.good();
  };

  bool sent = util::ReceiveRemoteUpdates(util::GetRemoteSocketPath(),
                                         std::string{model::kRemoteSubscribeQuery}, print);

  if (!sent) {
    std::cerr << "spectrum: there is no running instance to control\n";
    return EXIT_FAILURE;
  }

  return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* ********************************************************************************************** */

/**
 * @brief Send command to the running instance (or ask for its status, which is printed)
 *
 * @param command Command name
 * @param format Text with fields to print status (if empty, it is printed as received, in JSON)
 * @return EXIT_SUCCESS if command was accepted by the running instance, otherwise EXIT_FAILURE
 */
int send_remote_command(const std::string& command, const std::optional<std::string>& format) {
  if (command == model::kRemoteSubscribeQuery) return print_remote_status_updates(format);

  const bool is_query = command == model::kRemoteStatusQuery;
  std::string request = command;

  if (!is_query) {
    std::string error;
    auto parsed = model::ParseRemoteRequest(command, error);

    if (!parsed) {
      std::cerr << "spectrum: " << error;

      // Command exists when error is about its value, so there is no reason to list all of them
      if (!model::ParseRemoteCommand(command.substr(0, command.find(' ')))) {
        std::cerr << " (available: " << model::GetRemoteCommandNames() << ")";
      }

      std::cerr << "\n";
      return EXIT_FAILURE;
    }

    // Running instance has another working directory, so it must receive the full path. Anything
    // that does not exist here is sent as it is (e.g. name of a playlist)
    if (auto* target = std::get_if<std::string>(&parsed->value);
        target && !middleware::IsRemoteUrl(*target)) {
      std::error_code failure;

      if (auto path = std::filesystem::absolute(*target, failure);
          !failure && std::filesystem::exists(path, failure)) {
        *target = path.lexically_normal().string();
      }
    }

    // Send it in a single format, no matter how it was written
    std::ostringstream text;
    text << *parsed;
    request = text.str();
  }

  auto reply = util::SendRemoteRequest(util::GetRemoteSocketPath(), request);

  if (!reply) {
    std::cerr << "spectrum: there is no running instance to control\n";
    return EXIT_FAILURE;
  }

  if (is_query) {
    // Anything other than status is an error message
    auto status = model::format_status(*reply, format);

    if (!status) {
      std::cerr << "spectrum: " << *reply << "\n";
      return EXIT_FAILURE;
    }

    std::cout << *status << "\n";
    return EXIT_SUCCESS;
  }

  if (*reply != kRemoteReplyOk) {
    std::cerr << "spectrum: " << *reply << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/* ********************************************************************************************** */

int main(int argc, char** argv) {
  // In case of getting some unexpected argument or some other error: do not execute the program
  Settings options;
  if (!parse(argc, argv, options)) {
    return EXIT_SUCCESS;
  }

  // Instead of starting a new instance, just send command to the one already running
  if (options.remote_command)
    return send_remote_command(*options.remote_command, options.remote_format);

  // Write some information useful to understand any issue reported from this log
  util::Logger::SetThreadName("ui");
  util::FileHandler file_handler;
  const auto terminal_size = ftxui::Terminal::Size();

  INFO("Starting spectrum version=", SPECTRUM_VERSION);
  INFO("Options: log=", std::quoted(options.log_path), " verbose=", options.verbose_logging,
       " directory=", std::quoted(options.initial_dir));

  // Playlists and settings were saved in another directory by older versions
  file_handler.MigrateLegacyFiles();

  INFO("Files: playlists=", std::quoted(file_handler.GetPlaylistsPath()),
       " settings=", std::quoted(file_handler.GetSettingsPath()));
  INFO("Terminal size=", terminal_size.dimx, "x", terminal_size.dimy);

  // Settings from last run that are used by player since the beginning (audio output device chosen
  // by user and browser whose cookies may be sent when streaming songs)
  model::Settings settings;
  file_handler.ParseSettings(settings);

  // Create and initialize a new player
  auto player = audio::Player::Create(options.verbose_logging, settings);

  // Create and initialize a new terminal window
  auto terminal = interface::Terminal::Create(options.initial_dir);

  // Use terminal maximum width as input to decide how many bars should display on audio visualizer
  int number_bars = terminal->CalculateNumberBars();

  // Create and initialize a new middleware for terminal and player
  auto middleware = middleware::MediaController::Create(terminal, player, number_bars);

  // Register callbacks to Terminal and Player
  terminal->RegisterPlayerNotifier(middleware);
  player->RegisterInterfaceNotifier(middleware);

  // Create a full-size screen and register callbacks
  ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();

  // Register callbacks
  terminal->RegisterEventSenderCallback([&screen](const ftxui::Event& e) {
    // Workaround: always set cursor as hidden
    // P.S.: sometimes when ftxui::Input is rendered, a blinking cursor appears at bottom-right
    static ftxui::Screen::Cursor cursor{.shape = ftxui::Screen::Cursor::Shape::Hidden};
    screen.SetCursor(cursor);

    screen.PostEvent(e);
  });

  terminal->RegisterExitCallback([&screen, &player, &middleware]() {
    player->Exit();
    middleware->Exit();
    screen.ExitLoopClosure()();
  });

  // Listen for commands sent by other instances (only the first instance running does it)
  auto remote = util::RemoteServer::Create(
      util::GetRemoteSocketPath(),
      [&terminal, &middleware, &file_handler](const std::string& request) -> std::string {
        // Status is read directly from this thread, without waiting for UI
        if (request == model::kRemoteStatusQuery) return model::to_json(middleware->GetStatus());

        // Not written for status, as it may be requested many times (e.g. by a status bar)
        INFO("Received remote command=", std::quoted(request));

        std::string error;
        auto command = model::ParseRemoteRequest(request, error);
        if (!command) return error;

        // Unlike the other commands, this one does not depend on anything from UI
        if (const auto* target = std::get_if<std::string>(&command->value); target) {
          auto playlist = middleware::CreateRemotePlaylist(*target, file_handler);
          if (!playlist) return "nothing to play was found for \"" + *target + "\"";

          terminal->SendEvent(interface::CustomEvent::NotifyPlaylistSelection(*playlist));
          return kRemoteReplyOk;
        }

        terminal->SendEvent(interface::CustomEvent::RunRemoteCommand(*command));
        return kRemoteReplyOk;
      },
      std::string{model::kRemoteSubscribeQuery});

  // Every change on status is sent to the instances that asked to receive it
  if (remote) {
    middleware->SetStatusListener(
        [&remote](const model::PlayerStatus& status) { remote->Publish(model::to_json(status)); });
  }

  // Events posted to screen are discarded while its loop does not exist, so create it and ask
  // terminal to handle any custom event sent in the meantime (e.g. a warning about something that
  // has failed on initialization, otherwise it would be shown only after the first key pressed)
  ftxui::Loop loop(&screen, terminal);
  screen.PostEvent(ftxui::Event::Custom);

  // Start GUI loop and clear screen after exit
  loop.Run();

  if (remote) {
    middleware->SetStatusListener(nullptr);
    remote->Stop();
  }
  screen.ResetPosition(true);

  return EXIT_SUCCESS;
}
