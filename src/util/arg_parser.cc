#include "util/arg_parser.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <regex>

namespace util {

Parser ArgumentParser::Configure(const ExpectedArguments& args) {
  // Simply extend the ArgumentParser class, as we do not want to expose the default constructor,
  // neither do we want to use std::make_unique explicitly calling operator new()
  struct MakeUniqueEnabler : public ArgumentParser {};
  auto parser = std::make_unique<MakeUniqueEnabler>();

  if (!args.empty()) parser->Add(args);

  return parser;
}

/* ********************************************************************************************** */

void ArgumentParser::Add(const ExpectedArguments& args) {
  // Lambda to compare with choices from help argument
  auto match_help = [](const std::string_view& choice) {
    return choice == "-h" || choice == "--help";
  };

  // Valid choice is a single dash followed by a single character (e.g. "-l"), or a double dash
  // followed by a word (e.g. "--log"), where word may contain letters, digits and dashes
  static const std::regex kValidChoice(R"(^(-[A-Za-z0-9]|--[A-Za-z0-9][A-Za-z0-9-]*)$)");

  // Insert expected arguments into internal cache
  for (const auto& arg : args) {
    // Check if argument matches some fields from default help argument
    if (auto matched = std::find_if(arg.choices.begin(), arg.choices.end(), match_help);
        arg.name == "help" || matched != std::end(arg.choices)) {
      throw parsing_error("Cannot override default help text");
    }

    if (expected_arguments_.find(arg) != expected_arguments_.end())
      throw parsing_error("Cannot configure duplicated argument");

    // Check choices (argument may have fewer choices than the maximum, leaving the others empty)
    bool has_choice = false;

    for (const auto& choice : arg.choices) {
      if (choice.empty()) continue;

      if (!std::regex_match(choice, kValidChoice)) {
        throw parsing_error("Invalid choice for argument (expected \"-x\" or \"--word\")");
      }

      auto same_choice = [&choice](const Argument& other) {
        return std::find(other.choices.begin(), other.choices.end(), choice) != other.choices.end();
      };

      if (std::any_of(expected_arguments_.begin(), expected_arguments_.end(), same_choice)) {
        throw parsing_error("Cannot configure duplicated choice for different arguments");
      }

      has_choice = true;
    }

    if (!has_choice) throw parsing_error("Cannot configure argument without choices");

    expected_arguments_.insert(arg);
  }
}

/* ********************************************************************************************** */

ParsedArguments ArgumentParser::Parse(int count, char** values) const {
  ParsedArguments opts;
  if (count == 1) return opts;

  int index = 1;
  while (index < count) {
    std::string argument{values[index]};

    // Print helper and finish
    if (argument == "-h" || argument == "--help") PrintHelpAndThrow();

    // Lambda to match argument choice
    auto match_choice = [&argument](const Argument& arg) {
      return std::find(arg.choices.begin(), arg.choices.end(), argument) != arg.choices.end();
    };

    // Find match in choices from expected arguments
    auto found = std::find_if(expected_arguments_.begin(), expected_arguments_.end(), match_choice);

    // Unexpected argument, print error and finish
    if (found == expected_arguments_.end()) PrintErrorAndThrow(argument);

    // Argument does not expect a value
    if (found->is_empty) {
      opts[found->name] = true;
    } else {
      // Value is missing (option is the last argument), print error and finish
      if (index + 1 >= count) PrintMissingValueAndThrow(argument);

      // Parse value for argument
      std::string value = values[++index];

      // Invalid value for argument, print error and finish
      if (value.rfind('-', 0) == 0 || value.empty()) PrintErrorAndThrow(argument, value);

      // Following values belong to the same argument, even the ones that look like an option
      // (e.g. a negative number), as long as they are not an expected one
      while (found->is_multiple && index + 1 < count) {
        argument = values[index + 1];

        if (argument == "-h" || argument == "--help" ||
            std::any_of(expected_arguments_.begin(), expected_arguments_.end(), match_choice))
          break;

        value += " " + argument;
        ++index;
      }

      // Everything is fine, should include into opts
      opts[found->name] = value;
    }

    ++index;
  }

  return opts;
}

/* ********************************************************************************************** */

int ArgumentParser::GetBiggestLength(const Filtered& args) const {
  int length = 0;

  // Iterate through all expected arguments and get the maximum word length found for choice
  for (const auto& arg : args) {
    int choice_length = 0;
    for (const auto& choice : arg.choices) choice_length += (int)choice.size();

    if (length < choice_length) length = choice_length;
  }

  return length;
}

/* ********************************************************************************************** */

void ArgumentParser::PrintHelpAndThrow() const {
  std::cout << "spectrum\n\n";
  std::cout << "A music player with a simple and intuitive terminal user interface.\n\n";
  std::cout << "Options:";

  auto biggest_choice = GetBiggestLength(expected_arguments_);

  for (const auto& arg : expected_arguments_) {
    std::cout << "\n\t";

    // Append choices into a single string
    std::string choices;
    for (const auto& choice : arg.choices) choices.append(choice + ", ");

    // Remove last comma+space
    if (choices.size() > 1) choices.erase(choices.size() - 2);

    std::cout << std::setw(biggest_choice) << std::left << choices;
    std::cout << "\t" << arg.description;
  }
  std::cout << "\n";

  throw parsing_error("Received command to print helper");
}

/* ********************************************************************************************** */

void ArgumentParser::PrintErrorAndThrow(const std::string& parsed) const {
  if (parsed.empty())
    std::cout << "spectrum: empty option\n";
  else
    std::cout << "spectrum: invalid option [" << parsed << "]\n";

  throw parsing_error("Received unexpected argument");
}

/* ********************************************************************************************** */

void ArgumentParser::PrintErrorAndThrow(const std::string& argument,
                                        const std::string& value) const {
  std::cout << "spectrum: invalid value(" << value << ") for option [" << argument << "]\n";

  throw parsing_error("Received unexpected value for argument");
}

/* ********************************************************************************************** */

void ArgumentParser::PrintMissingValueAndThrow(const std::string& argument) const {
  std::cout << "spectrum: missing value for option [" << argument << "]\n";

  throw parsing_error("Missing value for argument");
}

}  // namespace util
