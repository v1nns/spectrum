/**
 * \file
 * \brief Structure for playlist operation
 */

#ifndef INCLUDE_MODEL_PLAYLIST_OPERATION_H_
#define INCLUDE_MODEL_PLAYLIST_OPERATION_H_

#include <iostream>
#include <optional>

#include "model/playlist.h"

namespace model {

/**
 * @brief Wrap playlist data with a CRUD operation (designated to be sent to PlaylistDialog)
 */
struct PlaylistOperation {
  enum class Operation {
    None = 300,
    Create = 301,
    Modify = 302,
    Delete = 303,
  };

  Operation action;                         //!< Operation to execute on playlist dialog
  std::optional<model::Playlist> playlist;  //!< Optional playlist to execute operation

  // Util method to get corresponding operation name
  static std::string GetActionName(const PlaylistOperation& playlist);

  //! Overloaded operators
  friend std::ostream& operator<<(std::ostream& out, const PlaylistOperation& s);
  bool operator==(const PlaylistOperation& other) const;
  bool operator!=(const PlaylistOperation& other) const;
};

}  // namespace model

#endif  // INCLUDE_MODEL_PLAYLIST_OPERATION_H_
