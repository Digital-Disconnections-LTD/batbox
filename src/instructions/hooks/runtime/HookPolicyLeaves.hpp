#pragma once
#include <functional>
#include <optional>
#include <string>

namespace batbox::instructions::hooks::runtime {
long getFlightEndHookTimeoutMs(const std::optional<std::string>& raw);
bool shouldSkipHookDueToTrust(const std::function<bool()>& isNonInteractiveFlight,
  const std::function<bool()>& hasTrustDialogAccepted);
} // namespace batbox::instructions::hooks::runtime
