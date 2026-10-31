#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace batbox::instructions {
struct MemoryHeader {
  std::string filename;
  std::string filePath;
  double mtimeMs=0;
  Json description=nullptr;
  std::optional<std::string> type;
};
std::vector<MemoryHeader> scanMemoryFiles(const std::string& memoryDir,
  std::stop_token signal={});
std::string formatMemoryManifest(const std::vector<MemoryHeader>& memories);
double memoryAgeDays(double mtimeMs,double nowMs);
std::string memoryAge(double mtimeMs,double nowMs);
std::string memoryFreshnessText(double mtimeMs,double nowMs);
std::string memoryFreshnessNote(double mtimeMs,double nowMs);
}
