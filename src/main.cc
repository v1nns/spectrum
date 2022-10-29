/**
 * \file
 * \brief Main function
 */
#include <cstdlib>  // for EXIT_SUCCESS

#include "audio/player.h"                          // for Player
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "middleware/media_controller.h"           // for MediaController
#include "util/arg_parser.h"                       // for ArgumentParser
#include "view/base/terminal.h"                    // for Terminal

//! Command-line argument parsing
bool parse(int argc, char** argv) {
  // Create arguments expectation
  using util::Argument, util::Arguments, util::Expected, util::Parser;
  auto expected_args = Expected{
      Argument{
          .name = "log",
          .choices = {"-l", "--log"},
          .description = "Enable logging to specified path",
      },
  };

  try {
    // Configure argument parser and run to get parsed arguments (first=command name, second=value)
    Parser arg_parser = util::ArgumentParser::Configure(expected_args);
    Arguments parsed_args = arg_parser->Parse(argc, argv);

    // Check if contains log config
    if (parsed_args.find("log") != parsed_args.end()) {
      // TODO: Enable logging to specified path
    }

  } catch (...) {
    // Got some error while trying to parse, let ArgumentParser inform about the error on CLI
    return false;
  }

  return true;
}

/* ********************************************************************************************** */

int main(int argc, char** argv) {
  // In case of getting some unexpected argument or some other error:
  // Do not execute the program
  if (!parse(argc, argv)) {
    return EXIT_SUCCESS;
  }

  // Create and initialize a new player
  auto player = audio::Player::Create();

  // Create and initialize a new terminal window
  auto terminal = interface::Terminal::Create();

  // Create and initialize a new middleware for terminal and player
  auto middleware = middleware::MediaController::Create(terminal, player);

  // Create a full-size screen and register callbacks
  ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();

  terminal->RegisterEventSenderCallback([&](ftxui::Event e) { screen.PostEvent(e); });
  terminal->RegisterExitCallback([&]() { screen.ExitLoopClosure()(); });

  // Start graphical interface loop and clear screen after exit
  screen.Loop(terminal);
  screen.ResetPosition(true);

  return EXIT_SUCCESS;
}
