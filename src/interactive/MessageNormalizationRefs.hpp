#pragma once

#include "interactive/MessageOrder.hpp"
#include "messages/Messages.hpp"

namespace batbox::interactive {

// Applies normalization while retaining the original message reference
// for pass-through message types.
MessageRefs normalizeMessagesRefs(const MessageRefs& messages,
                                   messages::Services& services);

} // namespace batbox::interactive
