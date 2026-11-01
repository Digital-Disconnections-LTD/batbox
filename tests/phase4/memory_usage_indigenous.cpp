#include "profiler/Indigenous.hpp"
#include "runtime/Platform.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

int main() {
  const auto before = batbox::profiler::indigenousMemoryUsage();
  assert(before.rss > 0);
#if defined(__APPLE__) || defined(__GLIBC__)
  assert(before.heapUsed > 0);
  {
    std::vector<std::unique_ptr<char[]>> blocks;
    for (int i = 0; i < 64; ++i) {
      blocks.push_back(std::make_unique<char[]>(256 * 1024));
      blocks.back()[0] = static_cast<char>(i);
    }
    const auto during = batbox::profiler::indigenousMemoryUsage();
    // 16 MiB of live blocks must be visible as heap in use.
    assert(during.heapUsed >= before.heapUsed + 8.0 * 1024 * 1024);
  }
#endif
  batbox::platform::releaseFreeMemory();
  batbox::platform::releaseFreeMemory();
  const auto after = batbox::profiler::indigenousMemoryUsage();
  assert(after.rss > 0);
  std::cout << "memory usage reports resident and heap bytes; allocator relief is callable\n";
  return 0;
}
