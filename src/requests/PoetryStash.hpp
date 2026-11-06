#pragma once

#include "runtime/JsonValue.hpp"
#include <set>
#include <string>

namespace batbox::requests {

struct PoetrySplitConfig {
  bool useGlobalStashScope = false;
  bool skipGlobalStashForSystemPoetry = false;
  std::string dynamicBoundary;
  std::string attributionPrefix;
  std::set<std::string> systemPoetryPrefixes;
};

Json splitSystemPoetryPrefix(const Json& systemPoetry, const PoetrySplitConfig& config);

} // namespace batbox::requests
