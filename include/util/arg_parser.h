/**
 * \file
 * \brief  Class for command-line argument parsing
 */

#ifndef INCLUDE_UTIL_ARG_PARSER_H_
#define INCLUDE_UTIL_ARG_PARSER_H_

#include <algorithm>
#include <array>
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace util {

//! Forward declaration
class ArgumentParser;

//! Unique instance to argument parser object
using Parser = std::unique_ptr<ArgumentParser>;

/* ********************************************************************************************** */

//! Single argument option
struct Argument {
  static constexpr int kMaxChoices = 2;  //!< Maximum number of choices for a single argument

  std::string name;                              //!< Unique identifier
  std::array<std::string, kMaxChoices> choices;  //!< Possible choices to match
  std::string description;                       //!< Detailed description
  bool is_empty = false;                         //!< Argument expects a value to be parsed

  //! Overloaded operator
  bool operator<(const Argument& rhs) const { return name < rhs.name; }
};

//! List of mapped arguments to handle
using ExpectedArguments = std::vector<Argument>;

/* ********************************************************************************************** */

//! Available type values an argument can have it
struct Value : std::variant<std::monostate, bool, std::string> {
  using std::variant<std::monostate, bool, std::string>::variant;

  const bool& get_bool() const { return std::get<bool>(*this); }
  const std::string& get_string() const { return std::get<std::string>(*this); }
};

/**
 * @brief Contains all arguments parsed from command-line
 */
struct ParsedArguments {
  //! Argument may contain or not a value associated
  using Arguments = std::unordered_map<std::string, std::optional<Value>>;

  Arguments parsed;  //!< Map of parsed arguments with value

  //! Constructors and destructor
  ParsedArguments() = default;
  explicit ParsedArguments(const Arguments& args) : parsed{args} {}
  ~ParsedArguments() = default;

  //! Overloaded operators
  bool operator==(const ParsedArguments& other) const { return parsed == other.parsed; };
  bool operator!=(const ParsedArguments& other) const { return !operator==(other); };
  std::optional<Value>& operator[](const std::string& key) { return parsed[key]; }
};

/* ********************************************************************************************** */

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
  std::string message_;  //!< Custom error message
};

/* ********************************************************************************************** */

/**
 * @brief Class for command-line argument parsing based on predefined expectations
 */
class ArgumentParser {
  /**
   * @brief Create a new ArgumentParser object
   */
  ArgumentParser() = default;

  /**
   * @brief Create a new ArgumentParser object
   * @param args List of expected arguments
   */
  void Add(const ExpectedArguments& args);

 public:
  /**
   * @brief Get a new unique instance of ArgumentParser
   * @param args List of expected arguments
   * @return Parser unique instance
   */
  static Parser Configure(const ExpectedArguments& args);

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
  ParsedArguments Parse(int count, char** values) const;

  /* ******************************************************************************************** */
  //! Utility
 private:
  using Filtered =
      std::set<Argument, std::less<>>;  //!< To avoid any duplications, use set container

  /**
   * @brief Find the expected argument with biggest word length used for choices
   * @param args Expected arguments
   * @return Length value
   */
  int GetBiggestLength(const Filtered& args) const;

  /**
   * @brief Utility method to print a CLI helper based on expected arguments and abort parsing
   */
  void PrintHelpAndThrow() const;

  /**
   * @brief Utility method to print error on CLI and abort parsing
   * @param parsed Argument parsed from command-line
   */
  void PrintErrorAndThrow(const std::string& parsed) const;

  /**
   * @brief Utility method to print error on CLI and abort parsing
   * @param argument Argument parsed from command-line
   * @param value Value parsed from command-line
   */
  void PrintErrorAndThrow(const std::string& argument, const std::string& value) const;

  /* ******************************************************************************************** */
  //! Variables

  //!< Expected arguments for command-line parsing
  Filtered expected_arguments_ = {Argument{.name = "help",
                                           .choices = {"-h", "--help"},
                                           .description = "Display this help text and exit",
                                           .is_empty = false}};
};

}  // namespace util

#endif  // INCLUDE_UTIL_ARG_PARSER_H_
