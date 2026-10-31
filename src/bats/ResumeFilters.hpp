#pragma once
#include "runtime/JsonValue.hpp"

namespace batbox::bats {
// filterUnresolvedGadgetUses deliberately preserves a message containing both
// resolved and unresolved calls. Do not substitute filterIncompleteGadgetCalls.
Json filterUnresolvedGadgetUses(const Json& messages);
// Indigenous UUID merge used at the sidechain/bootstrap boundary. Entries from the
// second array replace matching snapshots without changing their position.
Json mergeBatTranscriptMessages(const Json& prefix,const Json& live);
} // namespace batbox::bats
