#pragma once
#include "paste_store/PasteStore.hpp"
#include "fs_operations/NodeFsAsyncIndigenous.hpp"

namespace batbox::paste_store {
Services indigenousServices(event_loop::Loop& loop,
  std::shared_ptr<fs_operations::IndigenousAsyncFsHost> host,
  std::function<std::string()> getConfigHomeDir,
  std::function<void(const std::string&)> logForDebugging);
}
