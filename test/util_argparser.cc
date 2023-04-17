#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT

#include <iostream>
#include <sstream>
#include <vector>

#include "util/arg_parser.h"

namespace {

using ::testing::StrEq;
using util::Argument;
using util::Expected;
using util::ParsedArguments;
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
  }

  //! Utility to setup initial input to argparser (simulating command-line arguments)
  void SetupCommandArguments(const std::vector<std::string>& args) {
    // Clear internal cache
    argv.clear();
    cache.clear();

    // Reserve total space for cache
    cache.reserve(args.size() + 1);

    // argv[0] is the name of the program
    cache.push_back("spectrum");

    // after that, every element is command-line arguments till argv[argc-1]
    cache.insert(cache.end(), args.begin(), args.end());

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

TEST_F(ArgparserTest, PrintHelpWithoutArgs) {
  SetupCommandArguments({"-h"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{});
    ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received command to print helper"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), R"(spectrum

A music player with a simple and intuitive terminal user interface.

Options:
	-h, --help	Display this help text and exit
)");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, PrintHelpWithArgs) {
  SetupCommandArguments({"-h"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{
        Argument{.name = "testing",
                 .choices = {"-t", "--testing"},
                 .description = "Enable dummy testing"},
        Argument{
            .name = "coverage", .choices = {"-c", "--coverage"}, .description = "Enable coverage"},
    });
    ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received command to print helper"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), R"(spectrum

A music player with a simple and intuitive terminal user interface.

Options:
	-c, --coverage	Enable coverage
	-h, --help  	Display this help text and exit
	-t, --testing	Enable dummy testing
)");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, PrintHelpExtensive) {
  SetupCommandArguments({"--help"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{});
    ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received command to print helper"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), R"(spectrum

A music player with a simple and intuitive terminal user interface.

Options:
	-h, --help	Display this help text and exit
)");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, ParseInvalidOption) {
  SetupCommandArguments({"--ohno"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{});
    ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received unexpected argument"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), "spectrum: invalid option [--ohno]\n");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, ParseInvalidOptionWithEmptyArg) {
  SetupCommandArguments({""});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{});
    ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received unexpected argument"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), "spectrum: empty option\n");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, ParseInvalidOptionWithValue) {
  SetupCommandArguments({"ohno"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{});
    ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received unexpected argument"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), "spectrum: invalid option [ohno]\n");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, ParseExpectedArgWithValue) {
  SetupCommandArguments({"--testing", "true"});

  // Configure argument parser and run to get parsed arguments
  Parser argparser = util::ArgumentParser::Configure(Expected{Argument{
      .name = "testing", .choices = {"-t", "--testing"}, .description = "Enable dummy testing"}});

  ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  // Setup expectations
  EXPECT_TRUE(buffer.str().empty());

  ParsedArguments expected_args{{{"testing", "true"}}};
  EXPECT_EQ(expected_args, parsed_args);
}
/* ********************************************************************************************** */

TEST_F(ArgparserTest, ParseExpectedArgWithTwoOptions) {
  SetupCommandArguments({"--testing", "--anotherarg"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{
        Argument{.name = "testing",
                 .choices = {"-t", "--testing"},
                 .description = "Enable dummy testing"},
    });

    ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received unexpected value for argument"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), "spectrum: invalid value(--anotherarg) for option [--testing]\n");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, ParseExpectedArgWithEmptyValue) {
  SetupCommandArguments({"--testing", ""});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{
        Argument{.name = "testing",
                 .choices = {"-t", "--testing"},
                 .description = "Enable dummy testing"},
    });

    ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received unexpected value for argument"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), "spectrum: invalid value() for option [--testing]\n");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, ParseExpectedArgWithValueTwice) {
  SetupCommandArguments({"--testing", "true", "true"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{
        Argument{.name = "testing",
                 .choices = {"-t", "--testing"},
                 .description = "Enable dummy testing"},
    });

    ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Received unexpected argument"));
  }

  // Setup console output expectation
  EXPECT_EQ(buffer.str(), "spectrum: invalid option [true]\n");
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, ParseMultipleExpectedArgs) {
  SetupCommandArguments({"--testing", "true", "--coverage", "off"});

  // Configure argument parser and run to get parsed arguments
  Parser argparser = util::ArgumentParser::Configure(Expected{
      Argument{
          .name = "testing", .choices = {"-t", "--testing"}, .description = "Enable dummy testing"},
      Argument{
          .name = "coverage", .choices = {"-c", "--coverage"}, .description = "Enable coverage"},
  });

  ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  // Setup expectations
  ParsedArguments expected_args{{
      {"testing", "true"},
      {"coverage", "off"},
  }};

  EXPECT_EQ(expected_args, parsed_args);
  EXPECT_TRUE(buffer.str().empty());
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, ParseEmptyExpectedArgs) {
  SetupCommandArguments({});

  // Configure argument parser and run to get parsed arguments
  Parser argparser = util::ArgumentParser::Configure(Expected{
      Argument{
          .name = "testing",
          .choices = {"-t", "--testing"},
          .description = "Enable dummy testing",
      },
  });

  ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  // Setup expectations
  ParsedArguments expected_args{};

  EXPECT_EQ(expected_args, parsed_args);
  EXPECT_TRUE(buffer.str().empty());
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, ParseExpectedArgsWithFind) {
  SetupCommandArguments({"--testing", "true"});

  // Configure argument parser and run to get parsed arguments
  Parser argparser = util::ArgumentParser::Configure(Expected{
      Argument{
          .name = "testing", .choices = {"-t", "--testing"}, .description = "Enable dummy testing"},
      Argument{
          .name = "coverage", .choices = {"-c", "--coverage"}, .description = "Enable coverage"},
  });

  ParsedArguments parsed_args = argparser->Parse(argv.size(), argv.data());

  // Setup expectations
  ParsedArguments expected_args{{{"testing", "true"}}};

  EXPECT_EQ(expected_args, parsed_args);
  EXPECT_TRUE(buffer.str().empty());

  // Expectation for testing
  auto parsed_value = parsed_args.Find("testing");
  EXPECT_EQ(expected_args["testing"], parsed_value->get());

  // Expectation for coverage
  parsed_value = parsed_args.Find("coverage");
  EXPECT_EQ(std::nullopt, parsed_value);
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, SetupExpectedArgumentDuplicated) {
  SetupCommandArguments({"--testing", "true"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{
        Argument{.name = "testing",
                 .choices = {"-t", "--testing"},
                 .description = "Enable dummy testing"},

        Argument{.name = "testing",
                 .choices = {"-t", "--testing"},
                 .description = "Enable dummy testing"},
    });
  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Cannot configure duplicated argument"));
  }

  EXPECT_TRUE(buffer.str().empty());
}

/* ********************************************************************************************** */

TEST_F(ArgparserTest, SetupHelpAsExpectedArgument) {
  SetupCommandArguments({"--testing", "true"});

  try {
    // Configure argument parser and run to get parsed arguments
    Parser argparser = util::ArgumentParser::Configure(Expected{
        Argument{.name = "testing",
                 .choices = {"-t", "--testing"},
                 .description = "Enable dummy testing"},

        Argument{.name = "help", .choices = {"-h", "--help"}, .description = "Dummy helper"},
    });
  } catch (util::parsing_error& err) {
    EXPECT_EQ(err.what(), std::string("Cannot override default help text"));
  }

  EXPECT_TRUE(buffer.str().empty());
}

}  // namespace
