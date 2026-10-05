/**
 * \file
 * \brief Main function
 */
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>

#include "audio/player.h"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/screen/terminal.hpp"
#include "middleware/media_controller.h"
#include "model/remote_command.h"
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
        },
    };

    // Configure argument parser and run to get parsed arguments
    Parser arg_parser = util::ArgumentParser::Configure(expected_args);
    ParsedArguments parsed_args = arg_parser->Parse(argc, argv);

    // Command is handled by the running instance, so nothing else is used (not even logging)
    if (auto& remote = parsed_args["remote"]; remote) {
      options.remote_command = remote->get_string();
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
 * @brief Send command to the running instance
 *
 * @param command Command name
 * @return EXIT_SUCCESS if command was accepted by the running instance, otherwise EXIT_FAILURE
 */
int send_remote_command(const std::string& command) {
  if (!model::ParseRemoteCommand(command)) {
    std::cerr << "spectrum: unknown command " << std::quoted(command)
              << " (available: " << model::GetRemoteCommandNames() << ")\n";
    return EXIT_FAILURE;
  }

  auto reply = util::SendRemoteRequest(util::GetRemoteSocketPath(), command);

  if (!reply) {
    std::cerr << "spectrum: there is no running instance to control\n";
    return EXIT_FAILURE;
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
  if (options.remote_command) return send_remote_command(*options.remote_command);

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

  // Create and initialize a new player
  auto player = audio::Player::Create(options.verbose_logging);

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
      util::GetRemoteSocketPath(), [&terminal](const std::string& request) -> std::string {
        auto command = model::ParseRemoteCommand(request);
        if (!command) return "unknown command \"" + request + "\"";

        terminal->SendEvent(interface::CustomEvent::RunRemoteCommand(*command));
        return kRemoteReplyOk;
      });

  // Start GUI loop and clear screen after exit
  screen.Loop(terminal);
  if (remote) remote->Stop();
  screen.ResetPosition(true);

  return EXIT_SUCCESS;
}
