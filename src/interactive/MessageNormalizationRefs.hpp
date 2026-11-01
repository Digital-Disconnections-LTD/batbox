#pragma once

#include "interactive/MessageOrder.hpp"
#include "messages/Messages.hpp"

namespace batbox::interactive {

// Applies normalization while retaining the original message reference
// for pass-through message types.
MessageRefs normalizeMessagesRefs(const MessageRefs& messages,
                                   messages::Services& services);
// Normalizes one message as normalizeMessagesRefs would at its position;
// isNewChain carries the chain state of the messages before it.
MessageRefs normalizeMessageRef(const MessageRef& message,
                                messages::Services& services, bool& isNewChain);

} // namespace batbox::interactive
