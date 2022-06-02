/**
 * \file
 * \brief  Struct with global data shared between graphical and audio threads
 */

#ifndef INCLUDE_MODEL_GLOBAL_RESOURCE_H_
#define INCLUDE_MODEL_GLOBAL_RESOURCE_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "model/song.h"

namespace model {

/**
 * @brief Plain data used by the whole multithreading application
 */
struct GlobalResource {
  std::mutex mutex;                  //!< Control access for these resources
  std::condition_variable cond_var;  //!< Control Audio thread execution

  //! These are flags used to notify Audio thread
  std::atomic<bool> play;  //!< Start playing song
  //   std::atomic<bool> resume; //!< Resume song
  //   std::atomic<bool> stop; //!< Stop playing song
  std::atomic<bool> exit;  //!< Exit from application

  std::shared_ptr<model::Song> curr_song;  //!< Current song playing

  /**
   * @brief Notify all threads locked by condition_variable to exit
   */
  void NotifyToExit() {
    exit.store(true);
    cond_var.notify_all();
  }
};

}  // namespace model
#endif  // INCLUDE_MODEL_GLOBAL_RESOURCE_H_