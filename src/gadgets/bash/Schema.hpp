#pragma once
#include "runtime/JsonValue.hpp"

namespace batbox::bash_gadget {
// Preprocess semantic numbers and booleans, then validate the strict Bash
// input object. The internal sed-preview field is accepted only when requested
// by the caller after permission approval.
Json parseBashInput(const Json& input, bool backgroundMissionsDisabled = false,
                    bool allowInternalSedEdit = false);
Json bashInputJSONSchema(double maxTimeoutMs, bool backgroundMissionsDisabled = false);
Json bashOutputJSONSchema();
}
