#include "flights/Load.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

using batbox::Json;
namespace fl = batbox::flights;

namespace {
double peakResidentBytes() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS counters{};
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) return 0;
  return static_cast<double>(counters.PeakWorkingSetSize);
#else
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#if defined(__APPLE__)
  return static_cast<double>(usage.ru_maxrss);
#else
  return static_cast<double>(usage.ru_maxrss) * 1024.0;
#endif
#endif
}
} // namespace

int main() {
  const auto path = std::filesystem::temp_directory_path() /
      ("batbox-load-memory-" + std::to_string(std::random_device{}()) + ".jsonl");
  const std::string body(900, 'r');
  std::size_t lines = 0;
  {
    std::ofstream out(path, std::ios::binary);
    std::string parent;
    while (static_cast<std::size_t>(out.tellp()) < 20u * 1024 * 1024) {
      const auto uuid = "00000000-0000-4000-8000-" + std::to_string(100000000000 + lines);
      Json entry = {{"type", lines % 2 ? "assistant" : "user"}, {"uuid", uuid},
          {"parentUuid", parent.empty() ? Json() : Json(parent)},
          {"sessionId", "flight"}, {"timestamp", "2026-11-01T00:00:00.000Z"},
          {"message", {{"role", lines % 2 ? "assistant" : "user"},
                       {"content", body + std::to_string(lines)}}}};
      out << entry.dump() << '\n';
      if (lines % 1000 == 0) out << "\n   \n{not json\n";
      parent = uuid;
      ++lines;
    }
  }
  const auto fileSize = static_cast<double>(std::filesystem::file_size(path));

  fl::LoadServices services;
  fl::bindIndigenousFileReads(services);
  services.disablePrecompactSkip = true;
  const double before = peakResidentBytes();
  auto loaded = fl::loadTranscriptFile(path.string(), true, services);
  const double after = peakResidentBytes();
  std::filesystem::remove(path);

  assert(loaded.messages.values().size() == lines);
  assert(loaded.leafUuids.size() == 1);
  const auto* last = loaded.messages.get(
      "00000000-0000-4000-8000-" + std::to_string(100000000000 + lines - 1));
  assert(last && last->at("message").at("content") == body + std::to_string(lines - 1));

  const double growth = after > before ? after - before : 0;
  std::cout << "loaded " << lines << " entries from " << fileSize / 1048576
            << " MiB; peak resident growth " << growth / 1048576 << " MiB ("
            << growth / fileSize << "x file size)\n";
  // Reading byte by byte into a growing string and parsing every line into
  // a Json array before copying entries into the map peaked above 6x.
  assert(growth <= 4.6 * fileSize);
  return 0;
}
