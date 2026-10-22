/**
 * \file
 * \brief  Send command from command-line to the running instance
 */

#ifndef INCLUDE_MIDDLEWARE_REMOTE_CLIENT_H_
#define INCLUDE_MIDDLEWARE_REMOTE_CLIENT_H_

#include <optional>
#include <ostream>
#include <string>

namespace middleware {

/**
 * @brief Send command to the running instance (or ask for its status, which is printed)
 *
 * @param path Socket path used by the running instance
 * @param command Command name (with its value, if any)
 * @param format Text with fields to print status (if empty, it is printed as received, in JSON)
 * @param out Where to print status
 * @param err Where to print errors
 * @return EXIT_SUCCESS if command was accepted by the running instance, otherwise EXIT_FAILURE
 */
int SendRemoteCommand(const std::string& path, const std::string& command,
                      const std::optional<std::string>& format, std::ostream& out,
                      std::ostream& err);

}  // namespace middleware
#endif  // INCLUDE_MIDDLEWARE_REMOTE_CLIENT_H_
