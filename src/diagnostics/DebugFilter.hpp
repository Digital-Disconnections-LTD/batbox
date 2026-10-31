#pragma once
#include <optional>
#include <string>
#include <vector>

namespace batbox::diagnostics {
struct DebugFilter {
  std::vector<std::string> include,exclude;
  bool isExclusive=false;
};
std::optional<DebugFilter> parseDebugFilter(const std::optional<std::string>& pattern);
std::vector<std::string> extractDebugCategories(const std::string& message);
bool shouldShowDebugCategories(const std::vector<std::string>& categories,
    const std::optional<DebugFilter>& filter);
bool shouldShowDebugMessage(const std::string& message,
    const std::optional<DebugFilter>& filter);
}
