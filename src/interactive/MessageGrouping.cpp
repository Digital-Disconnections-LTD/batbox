#include "interactive/MessageGrouping.hpp"
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace batbox::interactive {
namespace {
const Json* member(const Json& object, const char* key) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}
const Json* firstBlock(const Json& message) {
  const auto* inner = member(message, "message");
  const auto* content = inner ? member(*inner, "content") : nullptr;
  return content && content->is_array() && !content->empty() ? &(*content)[0] : nullptr;
}
std::string value(const Json& object, const char* key) {
  const auto* found = member(object, key);
  return found && found->is_string() ? found->get<std::string>() : "";
}
struct GadgetUseInfo { std::string messageId, gadgetUseId, gadgetName; };
std::optional<GadgetUseInfo> getGadgetUseInfo(const Json& message) {
  if (value(message, "type") != "assistant") return {};
  const auto* block = firstBlock(message);
  if (!block || value(*block, "type") != "tool_use") return {};
  const auto* inner = member(message, "message");
  return GadgetUseInfo{inner ? value(*inner, "id") : "",
                     value(*block, "id"), value(*block, "name")};
}
std::string key(const GadgetUseInfo& info) {
  return info.messageId + ":" + info.gadgetName;
}
Json arrayFromRefs(const MessageRefs& refs) {
  Json output = Json::array();
  for (const auto& ref : refs) output.push_back(*ref);
  return output;
}
} // namespace

RenderItemsRef asRenderItems(const MessageRefs& messages) {
  auto items = std::make_shared<RenderItems>();
  items->reserve(messages.size());
  for (const auto& message : messages) items->push_back({message, {}, {}, {}});
  return items;
}

RenderItemsRef applyGrouping(const RenderItemsRef& messages,
                             const GroupingGadgets& gadgets, bool verbose) {
  if (!messages || !gadgets) throw std::invalid_argument("Grouping messages and Gadgets required");
  if (verbose) return messages;
  if (!gadgets->stashedNames) {
    gadgets->stashedNames.emplace();
    for (const auto& gadget : gadgets->gadgets)
      if (gadget.renderGroupedGadgetUse) gadgets->stashedNames->insert(gadget.name);
  }
  const auto& groupedNames = *gadgets->stashedNames;
  std::unordered_map<std::string, MessageRefs> groups;
  for (const auto& item : *messages) {
    if (!item.message) continue;
    const auto info = getGadgetUseInfo(*item.message);
    if (info && groupedNames.contains(info->gadgetName))
      groups[key(*info)].push_back(item.message);
  }
  std::unordered_map<std::string, MessageRefs> valid;
  std::unordered_set<std::string> groupedIds;
  for (const auto& [groupKey, members] : groups) {
    if (members.size() < 2) continue;
    valid.emplace(groupKey, members);
    for (const auto& message : members) {
      const auto info = getGadgetUseInfo(*message);
      if (info) groupedIds.insert(info->gadgetUseId);
    }
  }
  std::unordered_map<std::string, MessageRef> resultById;
  for (const auto& item : *messages) {
    if (!item.message || value(*item.message, "type") != "user") continue;
    const auto* inner = member(*item.message, "message");
    const auto* content = inner ? member(*inner, "content") : nullptr;
    if (!content || !content->is_array()) continue;
    for (const auto& block : *content) {
      const auto id = value(block, "tool_use_id");
      if (value(block, "type") == "tool_result" && groupedIds.contains(id))
        resultById[id] = item.message;
    }
  }
  auto output = std::make_shared<RenderItems>();
  std::unordered_set<std::string> emitted;
  for (const auto& item : *messages) {
    if (!item.message) continue;
    const auto info = getGadgetUseInfo(*item.message);
    if (info) {
      const auto groupKey = key(*info);
      const auto group = valid.find(groupKey);
      if (group != valid.end()) {
        if (emitted.insert(groupKey).second) {
          const auto& first = group->second.front();
          MessageRefs results;
          for (const auto& assistant : group->second) {
            const auto id = getGadgetUseInfo(*assistant)->gadgetUseId;
            const auto found = resultById.find(id);
            if (found != resultById.end()) results.push_back(found->second);
          }
          const auto* stamp = member(*first, "timestamp");
          Json shell{{"type", "grouped_tool_use"}, {"toolName", info->gadgetName},
                     {"uuid", "grouped-" + value(*first, "uuid")},
                     {"messageId", info->messageId}};
          if (stamp) shell["timestamp"] = *stamp;
          output->push_back({std::make_shared<Json>(std::move(shell)),
                             group->second, std::move(results), first});
        }
        continue;
      }
    }
    if (value(*item.message, "type") == "user") {
      const auto* inner = member(*item.message, "message");
      const auto* content = inner ? member(*inner, "content") : nullptr;
      bool hasResult = false, allGrouped = true;
      if (content && content->is_array()) for (const auto& block : *content) {
        if (value(block, "type") != "tool_result") continue;
        hasResult = true;
        if (!groupedIds.contains(value(block, "tool_use_id"))) allGrouped = false;
      }
      if (hasResult && allGrouped) continue;
    }
    output->push_back(item);
  }
  return output;
}

Json materializeRenderItem(const RenderItem& item) {
  if (!item.message) return nullptr;
  if (item.collapsedItems) {
    Json result = *item.message;
    result["messages"] = Json::array();
    for (const auto& child : *item.collapsedItems)
      result["messages"].push_back(materializeRenderItem(child));
    result["displayMessage"] = materializeRenderItem(*item.displayItem);
    return result;
  }
  if (item.members.empty()) return *item.message;
  Json result = *item.message;
  result["messages"] = arrayFromRefs(item.members);
  result["results"] = arrayFromRefs(item.results);
  result["displayMessage"] = *item.displayMessage;
  return result;
}
} // namespace batbox::interactive
