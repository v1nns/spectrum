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
 * @brief Custom exception for error handling within ArgumentParser
 */
class parsing_error : public std::exception {
 public:
  /**
   * @brief Create a new parsing_error object
   * @param msg Error message
   */
  explicit parsing_error(const std::string& msg) : message_(msg) {}

  /**
   * @brief Return custom error message description about this parser exception
   * @return Error message description
   */
  const char* what() const noexcept override { return message_.c_str(); }

 private:
  std::string message_;  //!< Custom message
};

/* ********************************************************************************************** */

/**
 * @brief Class for command-line argument parsing based on predefined expectations
 */
class ArgumentParser {
 private:
  /**
   * @brief Create a new ArgumentParser object
   * @param args List of expected arguments
   */
  explicit ArgumentParser(const Expected& args)
      : arguments_{args} {
            // TODO: maybe replace vector by set, to avoid duplicated arguments
        };

 public:
  /**
   * @brief Get a new unique instance of ArgumentParser
   * @param args List of expected arguments
   * @return Parser unique instance
   */
  static Parser Configure(const Expected& args) {
    // Simply extend the ArgumentParser class, as we do not want to expose the default constructor,
    // neither do we want to use std::make_unique explicitly calling operator new()
    struct MakeUniqueEnabler : public ArgumentParser {
      explicit MakeUniqueEnabler(const Expected& args) : ArgumentParser(args) {}
    };
    return std::make_unique<MakeUniqueEnabler>(args);
  }

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
        throw parsing_error("Received command to print helper");
      }

      // Find match in expected arguments
      auto found =
          std::find_if(arguments_.begin(), arguments_.end(), [&argument](const Argument& arg) {
            return std::find(arg.choices.begin(), arg.choices.end(), argument) != arg.choices.end();
          });

      if (found == arguments_.end()) {
        PrintError(argument);
        throw parsing_error("Received unexpected argument");
      }

      // Get value for expected argument (for the first version, always expected value for argument)
      std::string value{values[++index]};
      if (value.rfind('-', 0) == 0 || value.empty()) {
        PrintError(argument, value);
        throw parsing_error("Received unexpected value for argument");
      }

      // Everything is fine, should include into opts
      opts[found->name] = value;
      ++index;
    }

    return opts;
  }

  /* ******************************************************************************************** */
  //! Utility
 private:
  /**
   * @brief Utility method to print a CLI helper based on expected arguments
   */
  void PrintHelp() const {
    std::cout << "spectrum\n\n";
    std::cout << "A music player with a simple and intuitive terminal user interface.\n\n";
    std::cout << "Options:";

    for (const auto& arg : arguments_) {
      std::cout << "\n\t";

      // Append choices into a single string
      std::string choices;
      for (const auto& choice : arg.choices) choices.append(choice + ", ");

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
  void PrintError(const std::string& parsed) const {
    if (parsed.empty())
      std::cout << "spectrum: empty option\n";
    else
      std::cout << "spectrum: invalid option [" << parsed << "]\n";
  }

  /**
   * @brief Utility method to print error on CLI
   * @param argument Argument parsed from command-line
   * @param value Value parsed from command-line
   */
  void PrintError(const std::string& argument, const std::string& value) const {
    std::cout << "spectrum: invalid value(" << value << ") for option [" << argument << "]\n";
  }

  /* ******************************************************************************************** */
  //! Variables
  Expected arguments_;  //!< Expected arguments for command-line parsing
};

}  // namespace util

#endif  // INCLUDE_UTIL_ARG_PARSER_H_