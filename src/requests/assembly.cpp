#include "requests/assembly.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace batbox::requests {
namespace {
const Json& field(const Json& object, const char* key) {
  return value::property(object, key);
}
bool has(const Json& object, const char* key) {
  return object.is_object() && object.contains(key);
}
bool typeIs(const Json& block, const char* type) {
  return block.is_object() && field(block, "type") == type;
}
bool isMedia(const Json& block) {
  return typeIs(block, "image") || typeIs(block, "document");
}
bool isGadgetResult(const Json& block) { return typeIs(block, "tool_result"); }
bool isGadgetResultBlock(const Json& block) {
  return block.is_object() && has(block, "type") && typeIs(block, "tool_result") && has(block, "tool_use_id");
}
void require(bool available, const char* name) {
  if (!available) throw std::runtime_error(std::string("Missing request dependency: ") + name);
}
void insertBlockAfterGadgetResults(Json& content, const Json& block) {
  int lastGadgetResultIndex = -1;
  for (std::size_t i = 0; i < content.size(); ++i) {
    if (isGadgetResult(content[i])) lastGadgetResultIndex = static_cast<int>(i);
  }
  if (lastGadgetResultIndex >= 0) {
    const std::size_t insertPos = static_cast<std::size_t>(lastGadgetResultIndex + 1);
    content.insert(content.begin() + insertPos, block);
    if (insertPos == content.size() - 1) content.push_back(Json{{"type", "text"}, {"text", "."}});
  } else {
    const std::size_t insertIndex = content.empty() ? 0 : content.size() - 1;
    content.insert(content.begin() + insertIndex, block);
  }
}
Json deduplicateEdits(const Json& block, std::set<std::string>& seenDeleteRefs) {
  Json uniqueEdits = Json::array();
  for (const auto& edit : field(block, "edits")) {
    const std::string ref = field(edit, "cache_reference").get<std::string>();
    if (!seenDeleteRefs.insert(ref).second) continue;
    uniqueEdits.push_back(edit);
  }
  Json result = block;
  result["edits"] = std::move(uniqueEdits);
  return result;
}
} // namespace

Json getStashControl(const Dependencies& deps, const std::optional<std::string>& scope,
                     const std::optional<std::string>& querySource) {
  require(static_cast<bool>(deps.shouldUseOneHourStash), "shouldUseOneHourCache");
  Json result = {{"type", "ephemeral"}};
  if (deps.shouldUseOneHourStash(querySource)) result["ttl"] = "1h";
  if (scope && *scope == "global") result["scope"] = *scope;
  return result;
}

Json userMessageToMessageParam(const Json& message, bool addStash, bool enablePoetryStashing,
                               const std::optional<std::string>& querySource, const Dependencies& deps) {
  const Json& content = field(field(message, "message"), "content");
  if (addStash) {
    if (content.is_string()) {
      Json block = {{"type", "text"}, {"text", content}};
      if (enablePoetryStashing) block["cache_control"] = getStashControl(deps, {}, querySource);
      return Json{{"role", "user"}, {"content", Json::array({block})}};
    }
    Json result = Json::array();
    for (std::size_t i = 0; i < content.size(); ++i) {
      Json block = content[i];
      if (i == content.size() - 1 && enablePoetryStashing)
        block["cache_control"] = getStashControl(deps, {}, querySource);
      result.push_back(std::move(block));
    }
    return Json{{"role", "user"}, {"content", result}};
  }
  return Json{{"role", "user"}, {"content", content}};
}

Json assistantMessageToMessageParam(const Json& message, bool addStash, bool enablePoetryStashing,
                                    const std::optional<std::string>& querySource, const Dependencies& deps) {
  const Json& content = field(field(message, "message"), "content");
  if (addStash) {
    if (content.is_string()) {
      Json block = {{"type", "text"}, {"text", content}};
      if (enablePoetryStashing) block["cache_control"] = getStashControl(deps, {}, querySource);
      return Json{{"role", "assistant"}, {"content", Json::array({block})}};
    }
    Json result = Json::array();
    for (std::size_t i = 0; i < content.size(); ++i) {
      Json block = content[i];
      bool connectorText = false;
      if (i == content.size() - 1 && !typeIs(block, "thinking") && !typeIs(block, "redacted_thinking") && deps.connectorTextFeature) {
        require(static_cast<bool>(deps.isConnectorTextBlock), "isConnectorTextBlock");
        connectorText = deps.isConnectorTextBlock(block);
      }
      if (i == content.size() - 1 && !typeIs(block, "thinking") && !typeIs(block, "redacted_thinking") && !connectorText && enablePoetryStashing)
        block["cache_control"] = getStashControl(deps, {}, querySource);
      result.push_back(std::move(block));
    }
    return Json{{"role", "assistant"}, {"content", result}};
  }
  return Json{{"role", "assistant"}, {"content", content}};
}

Json stripExcessMediaItems(const Json& messages, double limit) {
  double toRemove = 0;
  for (const auto& msg : messages) {
    const Json& content = field(field(msg, "message"), "content");
    if (!content.is_array()) continue;
    for (const auto& block : content) {
      if (isMedia(block)) ++toRemove;
      const Json& nested = field(block, "content");
      if (isGadgetResult(block) && nested.is_array()) {
        for (const auto& item : nested) if (isMedia(item)) ++toRemove;
      }
    }
  }
  toRemove -= limit;
  if (toRemove <= 0) return messages;
  Json result = Json::array();
  for (const auto& msg : messages) {
    if (toRemove <= 0) { result.push_back(msg); continue; }
    const Json& content = field(field(msg, "message"), "content");
    if (!content.is_array()) { result.push_back(msg); continue; }
    const double before = toRemove;
    Json mapped = Json::array();
    for (const auto& block : content) {
      const Json& nested = field(block, "content");
      if (toRemove <= 0 || !isGadgetResult(block) || !nested.is_array()) { mapped.push_back(block); continue; }
      Json filtered = Json::array();
      for (const auto& item : nested) {
        if (toRemove > 0 && isMedia(item)) { --toRemove; continue; }
        filtered.push_back(item);
      }
      if (filtered.size() == nested.size()) mapped.push_back(block);
      else { Json copy = block; copy["content"] = std::move(filtered); mapped.push_back(std::move(copy)); }
    }
    Json stripped = Json::array();
    for (const auto& block : mapped) {
      if (toRemove > 0 && isMedia(block)) { --toRemove; continue; }
      stripped.push_back(block);
    }
    if (before == toRemove) result.push_back(msg);
    else { Json copy = msg; copy["message"]["content"] = std::move(stripped); result.push_back(std::move(copy)); }
  }
  return result;
}

Json addStashBreakpoints(const Json& messages, bool enablePoetryStashing,
                         const std::optional<std::string>& querySource, bool useStashedMC,
                         const Json& newStashEdits, const Json& pinnedEdits,
                         bool skipStashWrite, const Dependencies& deps) {
  const int markerIndex = skipStashWrite ? static_cast<int>(messages.size()) - 2 : static_cast<int>(messages.size()) - 1;
  Json result = Json::array();
  for (std::size_t index = 0; index < messages.size(); ++index) {
    const Json& msg = messages[index];
    const bool addStash = static_cast<int>(index) == markerIndex;
    result.push_back(field(msg, "type") == "user"
      ? userMessageToMessageParam(msg, addStash, enablePoetryStashing, querySource, deps)
      : assistantMessageToMessageParam(msg, addStash, enablePoetryStashing, querySource, deps));
  }
  if (!useStashedMC) return result;

  std::set<std::string> seenDeleteRefs;
  if (pinnedEdits.is_array()) {
    for (const auto& pinned : pinnedEdits) {
      const int index = field(pinned, "userMessageIndex").get<int>();
      if (index < 0 || static_cast<std::size_t>(index) >= result.size()) continue;
      Json& msg = result[static_cast<std::size_t>(index)];
      if (field(msg, "role") != "user") continue;
      if (!field(msg, "content").is_array()) msg["content"] = Json::array({Json{{"type", "text"}, {"text", msg["content"]}}});
      Json deduped = deduplicateEdits(field(pinned, "block"), seenDeleteRefs);
      if (!deduped["edits"].empty()) insertBlockAfterGadgetResults(msg["content"], deduped);
    }
  }
  if (!newStashEdits.is_null() && !result.empty()) {
    Json deduped = deduplicateEdits(newStashEdits, seenDeleteRefs);
    if (!deduped["edits"].empty()) {
      for (int i = static_cast<int>(result.size()) - 1; i >= 0; --i) {
        Json& msg = result[static_cast<std::size_t>(i)];
        if (field(msg, "role") != "user") continue;
        if (!field(msg, "content").is_array()) msg["content"] = Json::array({Json{{"type", "text"}, {"text", msg["content"]}}});
        insertBlockAfterGadgetResults(msg["content"], deduped);
        require(static_cast<bool>(deps.pinStashEdits), "pinCacheEdits");
        deps.pinStashEdits(static_cast<std::size_t>(i), newStashEdits);
        break;
      }
    }
  }
  if (enablePoetryStashing) {
    int lastCCMsg = -1;
    for (std::size_t i = 0; i < result.size(); ++i) {
      const Json& content = field(result[i], "content");
      if (!content.is_array()) continue;
      for (const auto& block : content) if (has(block, "cache_control")) lastCCMsg = static_cast<int>(i);
    }
    if (lastCCMsg >= 0) {
      for (int i = 0; i < lastCCMsg; ++i) {
        Json& msg = result[static_cast<std::size_t>(i)];
        if (field(msg, "role") != "user" || !field(msg, "content").is_array()) continue;
        for (auto& block : msg["content"]) {
          if (isGadgetResultBlock(block)) block["cache_reference"] = block["tool_use_id"];
        }
      }
    }
  }
  return result;
}

Json buildSystemPoetryBlocks(const Json& systemPoetry, bool enablePoetryStashing,
                             bool skipGlobalStashForSystemPoetry,
                             const std::optional<std::string>& querySource, const Dependencies& deps) {
  require(static_cast<bool>(deps.splitSystemPoetryPrefix), "splitSystemPromptPrefix");
  Json result = Json::array();
  for (const auto& block : deps.splitSystemPoetryPrefix(systemPoetry, skipGlobalStashForSystemPoetry)) {
    Json output = {{"type", "text"}, {"text", field(block, "text")}};
    if (enablePoetryStashing && !field(block, "cacheScope").is_null()) {
      output["cache_control"] = getStashControl(deps, field(block, "cacheScope").get<std::string>(), querySource);
    }
    result.push_back(std::move(output));
  }
  return result;
}

} // namespace batbox::requests
