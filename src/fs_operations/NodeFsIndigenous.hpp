#pragma once
#include "fs_operations/NodeFsSync.hpp"
#include <memory>

namespace batbox::fs_operations {
NodeFsSyncImports indigenousNodeFsSyncImports(
  std::shared_ptr<diagnostics::SlowOperations> slowOperations);
}
