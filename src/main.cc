#include <assert.h>

#include <memory>

#include "error_code.h"
#include "ui/module/file_info.h"
#include "ui/terminal.h"

using Terminal = std::unique_ptr<interface::Terminal>;

int main() {
  Terminal term = std::make_unique<interface::Terminal>();

  // Initialize screen
  int result = term->Init();
  assert(result == ERR_OK);

  // Add a new block
  std::unique_ptr<interface::Block> file{new interface::FileInfo{{0, 0}, {40, 22}}};
  term->AppendBlock(file);

  while (term->Tick()) {
    // do nothing in here
  };

  return ERR_OK;
}