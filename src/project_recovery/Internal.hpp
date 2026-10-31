#pragma once
#include "project_recovery/Recovery.hpp"
#include <string_view>

namespace batbox::project_recovery::detail {
std::optional<std::filesystem::path> startDirectory(const std::filesystem::path& cwd);
std::filesystem::path projectRoot(const std::filesystem::path& cwd);
bool plainDirectory(const std::filesystem::path& path);
bool plainFile(const std::filesystem::path& path);
struct TextSlice {
  std::string text;
  bool malformed=false,truncated=false;
};
TextSlice utf8Prefix(std::string_view text,std::size_t limit);
struct Template { const char* name;const char* content; };
const std::vector<Template>& defaults();
}
