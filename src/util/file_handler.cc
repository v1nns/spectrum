#include "util/file_handler.h"

#include <algorithm>
#include <exception>

#include "util/logger.h"

namespace util {

namespace internal {

//! Transform single character into lowercase
static void to_lower(char& c) { c = (char)std::tolower(c); }

/**
 * @brief Custom file sort algorithm
 * @param a Filename a
 * @param b Filename b
 * @return true if 'a' is alphabetically lesser than b, false otherwise
 */
static bool sort_files(const File& a, const File& b) {
  std::string lhs{a.filename()};
  std::string rhs{b.filename()};

  // Don't care if it is hidden (tried to make it similar to "ls" output)
  if (lhs.at(0) == '.') lhs.erase(0, 1);
  if (rhs.at(0) == '.') rhs.erase(0, 1);

  std::for_each(lhs.begin(), lhs.end(), to_lower);
  std::for_each(rhs.begin(), rhs.end(), to_lower);

  return lhs < rhs;
}

}  // namespace internal

/* ********************************************************************************************** */

bool FileHandler::ListFiles(const std::filesystem::path& dir_path, Files& parsed_files) {
  Files tmp;

  try {
    // Add all files from the given directory
    for (auto const& entry : std::filesystem::directory_iterator(dir_path)) {
      tmp.emplace_back(entry);
    }
  } catch (std::exception& e) {
    ERROR("Cannot access directory, exception=", e.what());
    return false;
  }

  // Sort list alphabetically (case insensitive)
  std::sort(tmp.begin(), tmp.end(), internal::sort_files);

  // Add option to go back one level
  tmp.emplace(tmp.begin(), "..");

  // Update structure with parsed files
  parsed_files.swap(tmp);

  return true;
}

}  // namespace util
