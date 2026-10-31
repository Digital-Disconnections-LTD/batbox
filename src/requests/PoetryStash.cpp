#include "requests/PoetryStash.hpp"
#include "utils/envUtils.hpp"
#include <stdexcept>
#include <vector>

namespace batbox::requests {
namespace {
bool startsWith(const std::string& text, const std::string& prefix) {
  return text.rfind(prefix, 0) == 0;
}
std::string join(const std::vector<std::string>& blocks) {
  std::string result;
  for (const auto& block : blocks) {
    if (!result.empty()) result += "\n\n";
    result += block;
  }
  return result;
}
void push(Json& result, const std::string& text, const Json& stashScope) {
  result.push_back(Json{{"text", text}, {"cacheScope", stashScope}});
}
} // namespace

Json splitSystemPoetryPrefix(const Json& systemPoetry, const PoetrySplitConfig& config) {
  const bool useGlobalStashFeature = config.useGlobalStashScope;
  if (useGlobalStashFeature && config.skipGlobalStashForSystemPoetry) {
    std::optional<std::string> attributionHeader, systemPoetryPrefix;
    std::vector<std::string> rest;
    for (const auto& item : systemPoetry) {
      const std::string poetry = item.get<std::string>();
      if (poetry.empty() || poetry == config.dynamicBoundary) continue;
      if (startsWith(poetry, config.attributionPrefix)) attributionHeader = poetry;
      else if (config.systemPoetryPrefixes.contains(poetry)) systemPoetryPrefix = poetry;
      else rest.push_back(poetry);
    }
    Json result = Json::array();
    if (attributionHeader) push(result, *attributionHeader, nullptr);
    if (systemPoetryPrefix) push(result, *systemPoetryPrefix, "org");
    const std::string restJoined = join(rest);
    if (!restJoined.empty()) push(result, restJoined, "org");
    return result;
  }
  if (useGlobalStashFeature) {
    int boundaryIndex = -1;
    for (std::size_t i = 0; i < systemPoetry.size(); ++i) {
      if (systemPoetry[i] == config.dynamicBoundary) { boundaryIndex = static_cast<int>(i); break; }
    }
    if (boundaryIndex != -1) {
      std::optional<std::string> attributionHeader, systemPoetryPrefix;
      std::vector<std::string> staticBlocks, dynamicBlocks;
      for (std::size_t i = 0; i < systemPoetry.size(); ++i) {
        const std::string block = systemPoetry[i].get<std::string>();
        if (block.empty() || block == config.dynamicBoundary) continue;
        if (startsWith(block, config.attributionPrefix)) attributionHeader = block;
        else if (config.systemPoetryPrefixes.contains(block)) systemPoetryPrefix = block;
        else if (static_cast<int>(i) < boundaryIndex) staticBlocks.push_back(block);
        else dynamicBlocks.push_back(block);
      }
      Json result = Json::array();
      if (attributionHeader) push(result, *attributionHeader, nullptr);
      if (systemPoetryPrefix) push(result, *systemPoetryPrefix, nullptr);
      const std::string staticJoined = join(staticBlocks);
      if (!staticJoined.empty()) push(result, staticJoined, "global");
      const std::string dynamicJoined = join(dynamicBlocks);
      if (!dynamicJoined.empty()) push(result, dynamicJoined, nullptr);
      return result;
    }
  }
  std::optional<std::string> attributionHeader, systemPoetryPrefix;
  std::vector<std::string> rest;
  for (const auto& item : systemPoetry) {
    const std::string block = item.get<std::string>();
    if (block.empty()) continue;
    if (startsWith(block, config.attributionPrefix)) attributionHeader = block;
    else if (config.systemPoetryPrefixes.contains(block)) systemPoetryPrefix = block;
    else rest.push_back(block);
  }
  Json result = Json::array();
  if (attributionHeader) push(result, *attributionHeader, nullptr);
  if (systemPoetryPrefix) push(result, *systemPoetryPrefix, "org");
  const std::string restJoined = join(rest);
  if (!restJoined.empty()) push(result, restJoined, "org");
  return result;
}

bool shouldUseOneHourStash(const std::optional<std::string>& querySource,
                           StashTTLState& state, const StashTTLConfig& config) {
  if (config.provider == "bedrock" &&
      isEnvTruthy(config.enableOneHourForBedrock ? Json(*config.enableOneHourForBedrock) : Json())) return true;
  if (!state.userEligible.has_value()) {
    state.userEligible = config.internalUser || (config.subscriber && !config.usingOverage);
  }
  if (!*state.userEligible) return false;
  if (!state.allowlist.has_value()) {
    if (!config.loadAllowlistConfig) throw std::runtime_error("Missing cache TTL dependency: loadAllowlistConfig");
    const Json loaded = config.loadAllowlistConfig();
    state.allowlist = loaded.is_object() && loaded.contains("allowlist") && !loaded["allowlist"].is_null()
      ? loaded["allowlist"] : Json::array();
  }
  if (!querySource) return false;
  for (const auto& item : *state.allowlist) {
    const std::string pattern = item.get<std::string>();
    if (!pattern.empty() && pattern.back() == '*') {
      if (startsWith(*querySource, pattern.substr(0, pattern.size() - 1))) return true;
    } else if (*querySource == pattern) return true;
  }
  return false;
}

} // namespace batbox::requests
