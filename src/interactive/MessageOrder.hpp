#pragma once

#include "interactive/PresentationState.hpp"
#include "runtime/OrderedStringSet.hpp"
#include <optional>
#include <string>
#include <vector>

namespace batbox::interactive {

using MessageRefs = std::vector<MessageRef>;

bool isGadgetUseRequestMessage(const Json& message);
bool isGadgetUseResultMessage(const Json& message);
bool isHookAttachmentMessage(const Json& message);
std::optional<std::string> getGadgetUseID(const Json& message);
runtime::OrderedStringSet getGadgetUseIDs(const MessageRefs& messages);
MessageRefs reorderMessagesInUI(const MessageRefs& messages,
                                 const MessageRefs& syntheticStreamingGadgetUses);

} // namespace batbox::interactive
