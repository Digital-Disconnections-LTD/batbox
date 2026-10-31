#pragma once

#include "interactive/MessageDecisions.hpp"
#include "runtime/OrderedStringSet.hpp"
#include <memory>

namespace batbox::interactive {

struct GadgetBlockRef {
  MessageRef owner;
  std::size_t index = 0;
  const Json& value() const;
};

struct MessageLookups : MessageDecisionLookups {
  std::unordered_map<std::string, std::shared_ptr<runtime::OrderedStringSet>> siblingGadgetUseIDs;
  std::unordered_map<std::string, MessageRefs> progressMessagesByGadgetUseID;
  std::unordered_map<std::string, MessageRef> gadgetResultByGadgetUseID;
  std::unordered_map<std::string, GadgetBlockRef> gadgetUseByGadgetUseID;
  std::size_t normalizedMessageCount = 0;
  GadgetIDs erroredGadgetUseIDs;
};

MessageLookups buildMessageLookups(const MessageRefs& normalizedMessages,
                                   const MessageRefs& messages);

} // namespace batbox::interactive
