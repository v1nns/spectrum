#include <cstdlib>
#include <memory>

#include "ui/base/terminal.h"
#include "ui/block/file_info.h"
#include "ui/block/list_directory.h"
#include "ui/common.h"

using Terminal =
    std::unique_ptr<interface::Terminal>;  //!< Smart pointer to have only one terminal instance

using Block = std::unique_ptr<interface::Block>;  //!< Smart pointer to hold a generic block

/* ********************************************************************************************** */

int main() {
  // Create a new terminal window
  Terminal term = std::make_unique<interface::Terminal>();

  // Initialize terminal screen
  term->Init();

  // Create new block and add it to terminal, like a "blockbuilder"
  using interface::screen_portion_t;
  Block file_list{new interface::ListDirectory{screen_portion_t{0, 0}, screen_portion_t{1, .7}}};
  term->AppendBlock(file_list);

  Block file_info{new interface::FileInfo{screen_portion_t{0, .7}, screen_portion_t{1, .3}}};
  term->AppendBlock(file_info);

  while (term->Tick()) {
    // do nothing in here, just enjoy the view
  };

  return EXIT_SUCCESS;
}