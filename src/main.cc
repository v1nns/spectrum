/**
 * \file
 * \brief Main function
 */
#include <cstdlib>
#include <string>

#include "audio/player.h"
#include "ftxui/component/screen_interactive.hpp"
#include "middleware/media_controller.h"
#include "util/arg_parser.h"
#include "util/logger.h"
#include "view/base/terminal.h"

/**
 * @brief A structure containing all available options to configure using command-line arguments
 */
struct Settings {
  std::string initial_dir = "";  //!< Initial directory to list in "files" block
  bool verbose_logging = false;  //!< Enable verbose log messages
};

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
            .description = "Enable logging to specified path",
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
    };

    // Configure argument parser and run to get parsed arguments
    Parser arg_parser = util::ArgumentParser::Configure(expected_args);
    ParsedArguments parsed_args = arg_parser->Parse(argc, argv);

    // Check if contains filepath for logging
    if (auto& logging_path = parsed_args["log"]; logging_path) {
      // Enable logging to specified path
      util::Logger::GetInstance().Configure(logging_path->get_string());
    }

    // Check if contains flag for verbose logging
    if (auto& verbose = parsed_args["verbose"]; verbose) {
      options.verbose_logging = verbose->get_bool();
    }

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

int main(int argc, char** argv) {
  // In case of getting some unexpected argument or some other error: do not execute the program
  Settings options;
  if (!parse(argc, argv, options)) {
    return EXIT_SUCCESS;
  }

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

  // Start GUI loop and clear screen after exit
  screen.Loop(terminal);
  screen.ResetPosition(true);

  return EXIT_SUCCESS;
}
