#include <assert.h>

#include <csignal>
#include <memory>

#include "error_code.h"
#include "ui/base/terminal.h"
#include "ui/block/file_info.h"
#include "ui/block/list_directory.h"
#include "ui/common.h"

using Terminal =
    std::unique_ptr<interface::Terminal>;  //!< Smart pointer to have only one terminal instance

using Block = std::unique_ptr<interface::Block>;  //!< Smart pointer to hold a generic block

/* ********************************************************************************************** */

volatile bool resize_screen = false;  //!< Global flag to control if must resize screen content

/**
 * @brief Global hook to filter received signals
 *
 * @param sig Signal event received
 */
void global_signal_hook(int sig) {
  // In case it is a resize event, must set related flag
  if (sig == SIGWINCH) resize_screen = true;

#if defined(__sun) && defined(__SVR4)
  // reinstall the hook each time it's executed
  signal(sig, global_signal_hook);
#endif  // __sun && __SVR4
}

/* ********************************************************************************************** */

int main() {
  // Create a new terminal window
  Terminal term = std::make_unique<interface::Terminal>();

  // Initialize terminal screen
  int result = term->Init();
  assert(result == ERR_OK);

  // Create new block and add it to terminal
  using interface::screen_portion_t;
  Block file_list{new interface::ListDirectory{screen_portion_t{0, 0}, screen_portion_t{1, .7}}};
  term->AppendBlock(file_list);

  Block file_info{new interface::FileInfo{screen_portion_t{0, .7}, screen_portion_t{1, .3}}};
  term->AppendBlock(file_info);

  // Register hook to watch for received signals
  signal(SIGWINCH, global_signal_hook);

  while (term->Tick(resize_screen)) {
    // do nothing in here
  };

  return ERR_OK;
}