#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT

#include <iostream>
#include <sstream>
#include <vector>

#include "util/arg_parser.h"

namespace {

using ::testing::StrEq;
using util::Argument;
using util::Arguments;
using util::Expected;
using util::Parser;
using util::parsing_error;

/**
 * @brief Tests with ArgumentParser class
 */
class ArgparserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Save cout's buffer here
    sbuf = std::cout.rdbuf();

    // Redirect cout to our stringstream buffer or any other ostream
    std::cout.rdbuf(buffer.rdbuf());
  }

  void TearDown() override {
    // When done redirect cout to its old self
    std::cout.rdbuf(sbuf);

    // std::cout << buffer.str();
  }

  //! Utility to setup initial input to argparser (simulating command-line arguments)
  void SetupCommandArguments(const std::vector<std::string>& args) {
    cache.clear();

    // argv[0] is the name of the program
    cache.push_back("spectrum");

    // after that, every element is command-line arguments till argv[argc-1]
    cache.insert(cache.end(), args.begin(), args.end());
    cache.push_back("NULL");

    argv.clear();
    argv.reserve(cache.size());

    // Simulate argv just like when you execute the program from terminal
    for (auto& s : cache) argv.push_back(&s[0]);
  }

  /* ******************************************************************************************** */
  //! Command-line argument cache

  std::vector<std::string> cache;  //!< Vector containing command-line arguments
  std::vector<char*> argv;  //!< Vector of null-terminated strings (pointing to internal cache)

  /* ******************************************************************************************** */
  //! Capture std::cout output

  std::streambuf* sbuf;      //!< Underlying buffer used by std::cout
  std::stringstream buffer;  //!< Auxiliary buffer to capture output from std::cout
};

/* ********************************************************************************************** */

TEST_F(ArgparserTest, PrintHelp) {
  SetupCommandArguments({"-h"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{});
    Arguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received command to print helper"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), R"(spectrum

A music player with a simple and intuitive terminal user interface.

Options:
)");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, PrintHelpExtensive) {
  SetupCommandArguments({"--help"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{});
    Arguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received command to print helper"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), R"(spectrum

A music player with a simple and intuitive terminal user interface.

Options:
)");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, PrintInvalidOption) {
  SetupCommandArguments({"--ohno"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{});
    Arguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received unexpected argument"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), "spectrum: invalid option [--ohno]\n");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, PrintInvalidOptionWithEmptyArg) {
  SetupCommandArguments({""});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{});
    Arguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received unexpected argument"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), "spectrum: empty option\n");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, PrintAnotherInvalidOption) {
  SetupCommandArguments({"ohno"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{});
    Arguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received unexpected argument"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), "spectrum: invalid option [ohno]\n");
}



/* ********************************************************************************************** */

TEST_F(ArgparserTest, PrintInvalidValueForTwoArgs) {
  SetupCommandArguments({"--testing", "--anotherarg"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{
        Argument{.name = "testing",
                 .choices = {"-t", "--testing"},
                 .description = "Enable dummy testing"},
    });

    Arguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received unexpected value for argument"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), "spectrum: invalid value(--anotherarg) for option [--testing]\n");
}

/* ********************************************************************************************** */

/*
TODO: print helper along the options
TESTS:
- print helper with expected args
- unexpected arg with empty value
- expected arg with value
- expected arg with empty value
- expected arg with another arg
*/

}  // namespace
