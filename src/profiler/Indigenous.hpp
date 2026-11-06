#pragma once

namespace batbox::profiler {
struct MemoryUsage {double rss=0,heapUsed=0;};
MemoryUsage indigenousMemoryUsage();
}
