#pragma once

#include "interactive/MessageDecisions.hpp"
#include "interactive/StreamReducer.hpp"
#include "messages/Messages.hpp"

namespace batbox::interactive {

MessageRefs syntheticStreamingGadgetUseMessages(
    const std::vector<StreamingGadgetUse>& streamingGadgetUses,
    const GadgetIDs& inProgressGadgetUseIDs,
    const MessageRefs& normalizedMessages,
    messages::Services& messageServices);

} // namespace batbox::interactive
