#include <assert.h>

#include <memory>

#include "error_code.h"
#include "ui/module/file_info.h"
#include "ui/terminal.h"

using Terminal = std::unique_ptr<interface::Terminal>;

int main() {
  Terminal term = std::make_unique<interface::Terminal>();

  int result = term->Init();
  assert(result == ERR_OK);

  std::unique_ptr<interface::Block> file{new interface::FileInfo{{1, 1}, {40, 22}}};
  term->AppendBlock(file);

  while (term->Tick()) {
    // do nothing in here
  };

  return ERR_OK;
}