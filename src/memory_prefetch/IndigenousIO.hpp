#pragma once
#include "memory_prefetch/Prefetch.hpp"
#include "filesystem/Range.hpp"

namespace batbox::memory_prefetch {
event_loop::Deferred<std::vector<instructions::MemoryHeader>>
  scanMemoryFilesAsync(const std::string& dir,std::stop_token signal,
    event_loop::Loop& loop);
event_loop::Deferred<filesystem::ReadFileRangeResult>
  readMemoryRangeAsync(const std::string& path,std::stop_token signal,
    event_loop::Loop& loop);
} // namespace batbox::memory_prefetch
