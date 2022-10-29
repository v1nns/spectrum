/**
 * \file
 * \brief  Single-header for command-line argument parsing
 */

#ifndef INCLUDE_UTIL_ARG_PARSER_H_
#define INCLUDE_UTIL_ARG_PARSER_H_

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace util {

//! Forward declaration
class ArgumentParser;

//! Unique instance to argument parser object
using Parser = std::unique_ptr<ArgumentParser>;

//! Single argument option
struct Argument {
  std::string name;                  //!< Unique identifier
  std::vector<std::string> choices;  //!< Possible choices to match
  std::string description;           //!< Detailed description
};

//! List of mapped arguments to handle
using Expected = std::vector<Argument>;

//! Map of parsed argument with the value read
using Arguments = std::unordered_map<std::string, std::string>;

/**
 * @brief Class for command-line argument parsing based on predefined expectations
 */
class ArgumentParser {
 private:
  /**
   * @brief Create a new ArgumentParser object
   * @param args List of expected arguments
   */
  explicit ArgumentParser(Expected args) : arguments_{args} {};

 public:
  /**
   * @brief Get a new unique instance of ArgumentParser
   * @param args List of expected arguments
   * @return Parser unique instance
   */
  static Parser Configure(Expected args) { return Parser(new ArgumentParser(args)); }

  /**
   * @brief Destroy an ArgumentParser object
   */
  ~ArgumentParser() = default;

  /* ******************************************************************************************** */
  //! Remove these
  ArgumentParser(const ArgumentParser& other) = delete;             // copy constructor
  ArgumentParser(ArgumentParser&& other) = delete;                  // move constructor
  ArgumentParser& operator=(const ArgumentParser& other) = delete;  // copy assignment
  ArgumentParser& operator=(ArgumentParser&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Parse a list of command line arguments into a set of program options
   * @return A map containing all parsed arguments where key is the argument identifier and value is
   * the value read for that argument
   */
  Arguments Parse(int count, char** values) {
    Arguments opts;
    if (count == 1) return opts;

    int index = 1;
    while (index < count) {
      std::string argument{values[index]};

      if (argument == "-h" || argument == "--help") {
        PrintHelp();
        throw std::runtime_error("Received command to print helper");
      };

      // Find match in expected arguments
      auto found =
          std::find_if(arguments_.begin(), arguments_.end(), [argument](const Argument& arg) {
            return std::find(arg.choices.begin(), arg.choices.end(), argument) != arg.choices.end();
          });

      if (found == arguments_.end()) {
        PrintError(argument);
        throw std::runtime_error("Received unexpected argument");
      }

      // Get value for expected argument (for the first version, always expected value for argument)
      index++;
      std::string value{values[index]};
      if (value.rfind('-', 2) == 0 || value.empty()) {
        PrintError(argument, value);
        throw std::runtime_error("Received unexpected value for argument");
      }

      // Everything is fine, should include into opts
      opts[found->name] = value;

      index++;
    }

    return opts;
  }

  /* ******************************************************************************************** */
  //! Utility
 private:
  /**
   * @brief Utility method to print a CLI helper based on expected arguments
   */
  void PrintHelp() {
    std::cout << "spectrum\n\n";
    std::cout << "Options:";

    for (auto&& arg : arguments_) {
      std::cout << "\n\t";

      // Append choices into a single string
      std::string choices;
      for (auto&& choice : arg.choices) choices.append(choice + ", ");

      // Remove last comma+space
      if (choices.size() > 1) choices.erase(choices.size() - 2);

      std::cout << choices << "\t" << arg.description;
    }

    std::cout << std::endl;
  }

  /**
   * @brief Utility method to print error on CLI
   * @param parsed Argument parsed from command-line
   */
  void PrintError(const std::string& parsed) {
    std::cout << "spectrum: invalid option [" << parsed << "]\n";
  }

  /**
   * @brief Utility method to print error on CLI
   * @param argument Argument parsed from command-line
   * @param value Value parsed from command-line
   */
  void PrintError(const std::string& argument, const std::string& value) {
    std::cout << "spectrum: invalid value for option [" << argument << " " << value << "]\n";
  }

  /* ******************************************************************************************** */
  //! Variables
 private:
  Expected arguments_;  //!< Expected arguments for command-line parsing
};

}  // namespace util

#endif  // INCLUDE_UTIL_ARG_PARSER_H_