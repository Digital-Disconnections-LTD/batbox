#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::model {

struct CompletionState {
  bool stashedMCEnabled = false;
  std::optional<std::string> streamRequestId;
  const Json* options = nullptr;
};

struct CompletionServices {
  std::function<bool()> stashedMicrocompactFeature;
  std::function<void()> markGadgetsSentToAPIState;
  std::function<bool()> hasBatContext;
  std::function<void(const std::string&)> setLastMainRequestId;
};

// Apply query completion state updates after cleanup. This
// whole segment must run on the same owner thread as the surrounding query.
void completeQueryModel(const CompletionState& state,
                        const CompletionServices& services);

} // namespace batbox::model
