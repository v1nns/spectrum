/**
 * \file
 * \brief  Interface class for notify interface with information
 */

#ifndef INCLUDE_VIEW_BASE_INTERFACE_NOTIFIER_H_
#define INCLUDE_VIEW_BASE_INTERFACE_NOTIFIER_H_

//! Forward declaration
namespace model {
class Song;
}

namespace interface {

/**
 * @brief Interface class to notify interface with updated information
 */
class InterfaceNotifier {
 public:
  /**
   * @brief Construct a new Interface Notifier object
   */
  InterfaceNotifier() = default;

  /**
   * @brief Destroy the Interface Notifier object
   */
  virtual ~InterfaceNotifier() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Notify UI with detailed information from the parsed song
   * @param info Detailed audio information from previously file selected
   */
  virtual void NotifySongInformation(const model::Song& info) = 0;

  /**
   * @brief Notify UI to clear any info about the previously song that was playing
   */
  virtual void ClearSongInformation() = 0;
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_INTERFACE_NOTIFIER_H_