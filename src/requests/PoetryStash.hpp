#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
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

struct StashTTLState {
  std::optional<bool> userEligible;
  std::optional<Json> allowlist;
};

struct StashTTLConfig {
  std::string provider;
  std::optional<std::string> enableOneHourForBedrock;
  bool internalUser = false;
  bool subscriber = false;
  bool usingOverage = false;
  std::function<Json()> loadAllowlistConfig;
};

bool shouldUseOneHourStash(const std::optional<std::string>& querySource,
                           StashTTLState& state, const StashTTLConfig& config);

} // namespace batbox::requests
