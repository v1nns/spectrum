/**
 * \file
 * \brief  Interface class to listen for actions from blocks
 */

#ifndef INCLUDE_VIEW_BASE_ACTION_LISTENER_H_
#define INCLUDE_VIEW_BASE_ACTION_LISTENER_H_

#include <filesystem>

//! Forward declaration
namespace model {
class Song;
}

namespace interface {

/**
 * @brief Interface class to receive an interface action
 */
class ActionListener {
 public:
  /* ******************************************************************************************** */
  //! Originated from UI

  virtual void NotifyFileSelection(const std::filesystem::path& file) = 0;
  virtual void ClearCurrentSong() = 0;

  /* ******************************************************************************************** */
  //! Originated from Audio Player

  virtual void NotifySongInformation(const model::Song& info) = 0;
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_ACTION_LISTENER_H_