#pragma once
#include "instructions/MemoryPath.hpp"

namespace batbox::memory_prefetch {
std::string getBatMemoryDir(const std::string& batType,
  const std::string& scope,const std::string& cwd,
  const instructions::MemoryPathServices& paths);
} // namespace batbox::memory_prefetch
