#pragma once
#include "history/Reader.hpp"
#include "fs_operations/NodeFsBytes.hpp"

namespace batbox::history {
HistoryReaderImports indigenousHistoryReaderImports(event_loop::Loop& loop,
  fs_operations::NodeFsBytesImports bytes,
  std::function<std::string()> getFlightId,
  std::function<std::string()> getConfigHomeDir,
  std::function<Json(const std::string&)> jsonParse,
  std::function<void(const std::string&)> logForDebugging);
}
