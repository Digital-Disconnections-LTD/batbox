#pragma once
#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include "fs_operations/NodeFsBytes.hpp"

namespace batbox::fs_operations {
NodeFsBytesImports indigenousNodeFsBytesImports(event_loop::Loop& loop,
  std::shared_ptr<IndigenousAsyncFsHost> host);
}
