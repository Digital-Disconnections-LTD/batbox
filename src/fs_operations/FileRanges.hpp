#pragma once
#include "fs_operations/NodeFsBytes.hpp"
#include <optional>

namespace batbox::fs_operations {
// Read a bounded range or tail of a file through the asynchronous byte services.
event_loop::Deferred<std::optional<Json>> readFileRange(
  const NodeFsBytesImports& imports,const std::string& path,
  std::size_t offset,std::size_t maxBytes);
event_loop::Deferred<Json> tailFile(const NodeFsBytesImports& imports,
  const std::string& path,std::size_t maxBytes);
}
