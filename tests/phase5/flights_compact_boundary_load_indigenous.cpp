#include "event_loop/Deferred.hpp"
#include "flights/AsyncLoad.hpp"
#include "flights/Load.hpp"
#include "flights/scanning/Scanning.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

using batbox::Json;
namespace fl = batbox::flights;
namespace el = batbox::event_loop;

namespace {
int failures = 0;
void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}
Json entry(const std::string& type, const std::string& uuid, const Json& parent,
           const std::string& text) {
  return {{"type", type}, {"uuid", uuid}, {"parentUuid", parent},
          {"sessionId", "flight"}, {"timestamp", "2026-11-01T00:00:00.000Z"},
          {"message", {{"role", type}, {"content", text}}}};
}
// The loaded shape a resume sees: every message in map order with its parent
// link, the leaves, and the metadata collected from the whole file.
Json shape(const fl::LoadedTranscript& loaded) {
  Json messages = Json::array();
  for (const auto& message : loaded.messages.values())
    messages.push_back({message.at("uuid"), message.value("parentUuid", Json()),
                        message.at("type")});
  return {{"messages", messages}, {"leaves", loaded.leafUuids},
          {"customTitles", loaded.customTitles}, {"tags", loaded.tags}};
}
fl::LoadedTranscript loadAsync(const std::string& path, std::size_t skipThreshold) {
  el::Loop loop;
  fl::AsyncLoadServices services;
  fl::bindIndigenousAsyncFileReads(loop, services);
  services.skipThreshold = skipThreshold;
  std::optional<fl::LoadedTranscript> value;
  std::exception_ptr error;
  fl::loadTranscriptFileAsync(loop, path, false, services)
      .then([&](const fl::LoadedTranscript* loaded, const std::exception_ptr& failure) {
        error = failure;
        if (loaded) value.emplace(*loaded);
      });
  loop.run();
  if (error) std::rethrow_exception(error);
  if (!value) throw std::runtime_error("async load did not finish");
  return *value;
}
fl::LoadedTranscript loadSync(const std::string& path, std::size_t skipThreshold) {
  fl::LoadServices services;
  fl::bindIndigenousFileReads(services);
  fl::scanning::bindIndigenousLoadScanners(services);
  services.skipThreshold = skipThreshold;
  return fl::loadTranscriptFile(path, false, services);
}
}  // namespace

int main() {
  char directory[] = "/tmp/batbox-compact-boundary-load-XXXXXX";
  if (!::mkdtemp(directory)) {
    std::cerr << "mkdtemp failed\n";
    return 1;
  }
  const std::filesystem::path root = directory;
  const auto path = (root / "flight.jsonl").string();
  {
    Json boundary = {{"type", "system"}, {"subtype", "compact_boundary"},
                     {"uuid", "b1"}, {"parentUuid", nullptr}, {"logicalParentUuid", "a1"},
                     {"sessionId", "flight"}, {"timestamp", "2026-11-01T00:00:01.000Z"},
                     {"content", "Conversation compacted"},
                     {"compactMetadata", {{"trigger", "manual"}, {"preTokens", 1200}}}};
    Json summary = entry("user", "s1", "b1", "Summary: earlier work");
    summary["isCompactSummary"] = true;
    summary["isVisibleInTranscriptOnly"] = true;
    std::ofstream out(path, std::ios::binary);
    out << entry("user", "u1", nullptr, "first question").dump() << '\n'
        << entry("assistant", "a1", "u1", "first answer").dump() << '\n'
        << Json{{"type", "custom-title"}, {"sessionId", "flight"}, {"customTitle", "Kept title"}}.dump() << '\n'
        << boundary.dump() << '\n'
        << summary.dump() << '\n'
        << entry("user", "u2", "s1", "second question").dump() << '\n'
        << entry("assistant", "a2", "u2", "second answer").dump() << '\n'
        << Json{{"type", "tag"}, {"sessionId", "flight"}, {"tag", "kept-tag"}}.dump() << '\n';
  }
  const Json titles = {{"flight", "Kept title"}};
  const Json tags = {{"flight", "kept-tag"}};
  // Small files are read whole: every message stays in the map, both chains
  // keep their parents, and each chain has its own leaf.
  const Json whole = {
      {"messages", Json::array({Json::array({"u1", nullptr, "user"}),
                                Json::array({"a1", "u1", "assistant"}),
                                Json::array({"b1", nullptr, "system"}),
                                Json::array({"s1", "b1", "user"}),
                                Json::array({"u2", "s1", "user"}),
                                Json::array({"a2", "u2", "assistant"})})},
      {"leaves", Json::array({"a1", "a2"})}, {"customTitles", titles}, {"tags", tags}};
  // Large files skip everything before the last boundary but still collect
  // the metadata written before it.
  const Json skipped = {
      {"messages", Json::array({Json::array({"b1", nullptr, "system"}),
                                Json::array({"s1", "b1", "user"}),
                                Json::array({"u2", "s1", "user"}),
                                Json::array({"a2", "u2", "assistant"})})},
      {"leaves", Json::array({"a2"})}, {"customTitles", titles}, {"tags", tags}};
  try {
    const std::size_t large = 1 << 30;
    check(shape(loadSync(path, large)) == whole, "sync whole-file load");
    check(shape(loadSync(path, 0)) == skipped, "sync load past the boundary");
    check(shape(loadAsync(path, large)) == whole, "async whole-file load");
    check(shape(loadAsync(path, 0)) == skipped, "async load past the boundary");
    const auto loaded = loadSync(path, 0);
    const auto* boundary = loaded.messages.get("b1");
    check(boundary && boundary->at("compactMetadata") ==
                          Json({{"trigger", "manual"}, {"preTokens", 1200}}),
          "boundary metadata unchanged");
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    ++failures;
  }
  std::error_code ignored;
  std::filesystem::remove_all(root, ignored);
  if (failures) return 1;
  std::cout << "compact boundary transcripts load the same on every path\n";
  return 0;
}
