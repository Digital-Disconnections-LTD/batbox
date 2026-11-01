#pragma once

#include "interactive/MessageOrder.hpp"
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace batbox::interactive {

struct GroupingGadget {
  std::string name;
  bool renderGroupedGadgetUse = false;
};
struct GroupingGadgetSet {
  std::vector<GroupingGadget> gadgets;
  // Capture this once per gadgets-array identity.
  std::optional<std::unordered_set<std::string>> stashedNames;
};
using GroupingGadgets = std::shared_ptr<GroupingGadgetSet>;

struct RenderItem;
using RenderItems = std::vector<RenderItem>;
using RenderItemsRef = std::shared_ptr<RenderItems>;

struct RenderItem {
  MessageRef message;
  // Synthetic groups retain the original mutable message references.
  MessageRefs members;
  MessageRefs results;
  MessageRef displayMessage;
  RenderItemsRef collapsedItems;
  std::shared_ptr<RenderItem> displayItem;
  RenderItem(MessageRef root, MessageRefs grouped = {}, MessageRefs groupedResults = {},
             MessageRef display = {}, RenderItemsRef collapsed = {},
             std::shared_ptr<RenderItem> collapsedDisplay = {})
      : message(std::move(root)), members(std::move(grouped)),
        results(std::move(groupedResults)), displayMessage(std::move(display)),
        collapsedItems(std::move(collapsed)), displayItem(std::move(collapsedDisplay)) {}
};

RenderItemsRef asRenderItems(const MessageRefs& messages);
RenderItemsRef applyGrouping(const RenderItemsRef& messages,
                             const GroupingGadgets& gadgets, bool verbose = false);
Json materializeRenderItem(const RenderItem& item);
// Read-only materialized view: a plain item shares its message, while groups
// and collapsed items are materialized into a new value.
std::shared_ptr<const Json> materializedView(const RenderItem& item);

} // namespace batbox::interactive
