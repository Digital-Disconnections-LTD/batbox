#pragma once

#include "interactive/MessageLookups.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <vector>

namespace batbox::interactive::cards_gadget_use {

struct GroupedResult {
  Json param;
  // Absent and present Json(null) values remain distinct.
  std::optional<Json> output;
};
struct GroupedGadgetUseEntry {
  Json param;
  bool isResolved=false,isError=false,isInProgress=false;
  Json progressMessages=Json::array();
  std::optional<GroupedResult> result;
};
using GroupedRenderer=std::function<Json(const gadgets::Gadget&,
  const std::vector<GroupedGadgetUseEntry>&,bool,const gadgets::Gadgets&)>;

// The callback receives entries and the live gadget roster.
// It can retain absent output separately from a present null output.
Json renderGroupedGadgetUse(const Json& groupedMessage,
                          const gadgets::Gadgets& availableGadgets,
                          const MessageLookups& lookups,
                          const GadgetIDs& inProgressGadgetUseIDs,
                          bool shouldAnimate,const GroupedRenderer& renderGrouped);
}
