#include "messages/Internal.hpp"
#include <algorithm>
#include <set>
#include <vector>

namespace batbox::messages {
using namespace internal;
namespace {
std::string diagnostic(const std::vector<const Json*>& messages) {
  std::vector<std::string> parts;
  for (std::size_t i = 0; i < messages.size(); ++i) {
    const Json& m = *messages[i];
    const std::string type = field(m, "type");
    if (type == "assistant") {
      std::vector<std::string> uses, server;
      for (const auto& b : m.at("message").at("content")) {
        if (isBlock(b, "tool_use")) uses.push_back(field(b, "id"));
        if (isBlock(b, "server_tool_use") || isBlock(b, "mcp_tool_use")) server.push_back(field(b, "id"));
      }
      std::string desc = "[" + std::to_string(i) + "] assistant(id=" + field(m.at("message"), "id") + ", tool_uses=[";
      for (std::size_t j = 0; j < uses.size(); ++j) { if (j) desc += ","; desc += uses[j]; }
      desc += "]";
      if (!server.empty()) {
        desc += ", server_tool_uses=[";
        for (std::size_t j = 0; j < server.size(); ++j) { if (j) desc += ","; desc += server[j]; }
        desc += "]";
      }
      parts.push_back(desc + ")");
    } else if (type == "user" && get(m.at("message"), "content").is_array()) {
      std::vector<std::string> results;
      for (const auto& b : m.at("message").at("content")) if (isBlock(b, "tool_result")) results.push_back(field(b, "tool_use_id"));
      if (!results.empty()) {
        std::string desc = "[" + std::to_string(i) + "] user(tool_results=[";
        for (std::size_t j = 0; j < results.size(); ++j) { if (j) desc += ","; desc += results[j]; }
        parts.push_back(desc + "])");
      } else parts.push_back("[" + std::to_string(i) + "] " + type);
    } else parts.push_back("[" + std::to_string(i) + "] " + type);
  }
  std::string joined;
  for (const auto& part : parts) { if (!joined.empty()) joined += "; "; joined += part; }
  return joined;
}
} // namespace

Json ensureGadgetResultPairing(Json messages, Services& services) {
  Json result = Json::array(); bool repaired = false;
  std::set<std::string> allSeenGadgetUseIds;
  // Messages that pass through unchanged are moved into result; the repair
  // diagnostic reads them back from there.
  std::vector<std::ptrdiff_t> movedTo(messages.size(), -1);
  const auto passThrough = [&](std::size_t index) {
    result.push_back(std::move(messages[index]));
    movedTo[index] = static_cast<std::ptrdiff_t>(result.size()) - 1;
  };
  for (std::size_t i = 0; i < messages.size(); ++i) {
    const Json& msg = messages[i];
    if (field(msg, "type") != "assistant") {
      if (field(msg, "type") == "user" && get(msg.at("message"), "content").is_array() &&
          (result.empty() || field(result.back(), "type") != "assistant")) {
        const Json& content = msg.at("message").at("content");
        const bool hasResult = std::any_of(content.begin(), content.end(),
          [](const Json& block) { return isBlock(block, "tool_result"); });
        Json stripped = Json::array();
        if (hasResult) for (const auto& block : content) if (!isBlock(block, "tool_result")) stripped.push_back(block);
        if (hasResult && stripped.size() != content.size()) {
          repaired = true;
          if (!stripped.empty() || result.empty()) {
            Json patched = msg;
            patched["message"]["content"] = !stripped.empty() ? stripped : Json::array({textBlock("[Orphaned tool result removed due to conversation resume]")});
            result.push_back(std::move(patched));
          }
          continue;
        }
      }
      passThrough(i); continue;
    }
    std::set<std::string> serverResultIds;
    const Json& original = msg.at("message").at("content");
    for (const auto& c : original) if (has(c, "tool_use_id") && get(c, "tool_use_id").is_string()) serverResultIds.insert(field(c, "tool_use_id"));
    std::set<std::string> seenGadgetUseIds; std::vector<std::string> gadgetUseIds; std::vector<bool> keep;
    for (const auto& block : original) {
      if (isBlock(block, "tool_use")) {
        const std::string id = field(block, "id");
        if (allSeenGadgetUseIds.count(id)) { repaired = true; keep.push_back(false); continue; }
        allSeenGadgetUseIds.insert(id); seenGadgetUseIds.insert(id); gadgetUseIds.push_back(id);
      }
      if ((isBlock(block, "server_tool_use") || isBlock(block, "mcp_tool_use")) && !serverResultIds.count(field(block, "id"))) {
        repaired = true; keep.push_back(false); continue;
      }
      keep.push_back(true);
    }
    const bool assistantContentChanged = std::find(keep.begin(), keep.end(), false) != keep.end();
    if (!assistantContentChanged) passThrough(i);
    else {
      Json finalContent = Json::array(); std::size_t index = 0;
      for (const auto& block : original) if (keep[index++]) finalContent.push_back(block);
      if (finalContent.empty()) finalContent.push_back(Json{{"type", "text"}, {"text", "[Tool use interrupted]"}, {"citations", Json::array()}});
      Json assistantMsg = msg;
      assistantMsg["message"]["content"] = std::move(finalContent);
      result.push_back(std::move(assistantMsg));
    }
    const Json* nextMsg = i + 1 < messages.size() ? &messages[i + 1] : nullptr;
    std::set<std::string> existingGadgetResultIds; bool hasDuplicateGadgetResults = false;
    if (nextMsg && field(*nextMsg, "type") == "user") {
      const Json& content = get(nextMsg->at("message"), "content");
      if (content.is_array()) for (const auto& block : content) if (isBlock(block, "tool_result")) {
        const std::string id = field(block, "tool_use_id");
        if (existingGadgetResultIds.count(id)) hasDuplicateGadgetResults = true;
        existingGadgetResultIds.insert(id);
      }
    }
    std::vector<std::string> missingIds, orphanedIds;
    for (const auto& id : gadgetUseIds) if (!existingGadgetResultIds.count(id)) missingIds.push_back(id);
    for (const auto& id : existingGadgetResultIds) if (!seenGadgetUseIds.count(id)) orphanedIds.push_back(id);
    if (missingIds.empty() && orphanedIds.empty() && !hasDuplicateGadgetResults) continue;
    repaired = true;
    Json syntheticBlocks = Json::array();
    for (const auto& id : missingIds) syntheticBlocks.push_back(Json{{"type", "tool_result"}, {"tool_use_id", id},
      {"content", SYNTHETIC_GADGET_RESULT_PLACEHOLDER}, {"is_error", true}});
    if (nextMsg && field(*nextMsg, "type") == "user") {
      const Json rawContent = get(nextMsg->at("message"), "content");
      Json content = rawContent.is_array() ? rawContent : Json::array({textBlock(str(rawContent))});
      if (!orphanedIds.empty() || hasDuplicateGadgetResults) {
        Json filtered = Json::array(); std::set<std::string> seenTrIds;
        for (const auto& block : content) {
          if (isBlock(block, "tool_result")) {
            const std::string id = field(block, "tool_use_id");
            if (std::find(orphanedIds.begin(), orphanedIds.end(), id) != orphanedIds.end()) continue;
            if (seenTrIds.count(id)) continue;
            seenTrIds.insert(id);
          }
          filtered.push_back(block);
        }
        content = std::move(filtered);
      }
      Json patchedContent = syntheticBlocks;
      for (const auto& block : content) patchedContent.push_back(block);
      if (!patchedContent.empty()) {
        Json patchedNext = *nextMsg; patchedNext["message"]["content"] = std::move(patchedContent); ++i;
        result.push_back(std::move(patchedNext));
      } else {
        ++i; result.push_back(createUserMessage(Json{{"content", NO_CONTENT_MESSAGE}, {"isMeta", true}}, services));
      }
    } else if (!syntheticBlocks.empty()) {
      result.push_back(createUserMessage(Json{{"content", syntheticBlocks}, {"isMeta", true}}, services));
    }
  }
  if (repaired) {
    std::vector<const Json*> originals(messages.size());
    for (std::size_t i = 0; i < originals.size(); ++i)
      originals[i] = movedTo[i] >= 0 ? &result[static_cast<std::size_t>(movedTo[i])] : &messages[i];
    const std::string desc = diagnostic(originals);
    if (!services.strictGadgetResultPairing) throw std::logic_error("strictToolResultPairing service not supplied");
    if (services.strictGadgetResultPairing()) throw std::runtime_error(
      "ensureToolResultPairing: tool_use/tool_result pairing mismatch detected (strict mode). "
      "Refusing to repair — would inject synthetic placeholders into model context. Message structure: " + desc + ". See inc-4977.");
    if (!services.logError) throw std::logic_error("logError service not supplied");
    services.logError("ensureToolResultPairing: repaired missing tool_result blocks (" +
      std::to_string(messages.size()) + " -> " + std::to_string(result.size()) + " messages). Message structure: " + desc);
  }
  return result;
}
} // namespace batbox::messages
