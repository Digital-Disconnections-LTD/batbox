#include "requests/PoetryStash.hpp"
#include <vector>

namespace batbox::requests {
namespace {
bool startsWith(const std::string& text, const std::string& prefix) {
  return text.rfind(prefix, 0) == 0;
}
std::string join(const std::vector<const std::string*>& blocks) {
  std::size_t length = 0;
  for (const auto* block : blocks) length += block->size() + 2;
  std::string result;
  result.reserve(length);
  for (const auto* block : blocks) {
    if (!result.empty()) result += "\n\n";
    result += *block;
  }
  return result;
}
void push(Json& result, std::string text, const Json& stashScope) {
  result.push_back(Json{{"text", std::move(text)}, {"cacheScope", stashScope}});
}
} // namespace

Json splitSystemPoetryPrefix(const Json& systemPoetry, const PoetrySplitConfig& config) {
  const bool useGlobalStashFeature = config.useGlobalStashScope;
  if (useGlobalStashFeature && config.skipGlobalStashForSystemPoetry) {
    const std::string* attributionHeader = nullptr;
    const std::string* systemPoetryPrefix = nullptr;
    std::vector<const std::string*> rest;
    for (const auto& item : systemPoetry) {
      const std::string& poetry = item.get_ref<const std::string&>();
      if (poetry.empty() || poetry == config.dynamicBoundary) continue;
      if (startsWith(poetry, config.attributionPrefix)) attributionHeader = &poetry;
      else if (config.systemPoetryPrefixes.contains(poetry)) systemPoetryPrefix = &poetry;
      else rest.push_back(&poetry);
    }
    Json result = Json::array();
    if (attributionHeader) push(result, *attributionHeader, nullptr);
    if (systemPoetryPrefix) push(result, *systemPoetryPrefix, "org");
    std::string restJoined = join(rest);
    if (!restJoined.empty()) push(result, std::move(restJoined), "org");
    return result;
  }
  if (useGlobalStashFeature) {
    int boundaryIndex = -1;
    for (std::size_t i = 0; i < systemPoetry.size(); ++i) {
      if (systemPoetry[i] == config.dynamicBoundary) { boundaryIndex = static_cast<int>(i); break; }
    }
    if (boundaryIndex != -1) {
      const std::string* attributionHeader = nullptr;
      const std::string* systemPoetryPrefix = nullptr;
      std::vector<const std::string*> staticBlocks, dynamicBlocks;
      for (std::size_t i = 0; i < systemPoetry.size(); ++i) {
        const std::string& block = systemPoetry[i].get_ref<const std::string&>();
        if (block.empty() || block == config.dynamicBoundary) continue;
        if (startsWith(block, config.attributionPrefix)) attributionHeader = &block;
        else if (config.systemPoetryPrefixes.contains(block)) systemPoetryPrefix = &block;
        else if (static_cast<int>(i) < boundaryIndex) staticBlocks.push_back(&block);
        else dynamicBlocks.push_back(&block);
      }
      Json result = Json::array();
      if (attributionHeader) push(result, *attributionHeader, nullptr);
      if (systemPoetryPrefix) push(result, *systemPoetryPrefix, nullptr);
      std::string staticJoined = join(staticBlocks);
      if (!staticJoined.empty()) push(result, std::move(staticJoined), "global");
      std::string dynamicJoined = join(dynamicBlocks);
      if (!dynamicJoined.empty()) push(result, std::move(dynamicJoined), nullptr);
      return result;
    }
  }
  const std::string* attributionHeader = nullptr;
  const std::string* systemPoetryPrefix = nullptr;
  std::vector<const std::string*> rest;
  for (const auto& item : systemPoetry) {
    const std::string& block = item.get_ref<const std::string&>();
    if (block.empty()) continue;
    if (startsWith(block, config.attributionPrefix)) attributionHeader = &block;
    else if (config.systemPoetryPrefixes.contains(block)) systemPoetryPrefix = &block;
    else rest.push_back(&block);
  }
  Json result = Json::array();
  if (attributionHeader) push(result, *attributionHeader, nullptr);
  if (systemPoetryPrefix) push(result, *systemPoetryPrefix, "org");
  std::string restJoined = join(rest);
  if (!restJoined.empty()) push(result, std::move(restJoined), "org");
  return result;
}

} // namespace batbox::requests
