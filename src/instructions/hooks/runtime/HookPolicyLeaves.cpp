#include "instructions/hooks/runtime/HookPolicyLeaves.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
long getFlightEndHookTimeoutMs(const std::optional<std::string>& raw){
  const auto parsed=raw&& !raw->empty()?value::parseInt10(*raw):
    std::numeric_limits<double>::quiet_NaN();
  return std::isfinite(parsed)&&parsed>0?static_cast<long>(parsed):1500;
}
bool shouldSkipHookDueToTrust(const std::function<bool()>& isNonInteractiveFlight,
  const std::function<bool()>& hasTrustDialogAccepted){
  if(!isNonInteractiveFlight||!hasTrustDialogAccepted)
    throw std::invalid_argument("Hook trust imports required");
  const auto isInteractive=!isNonInteractiveFlight();
  if(!isInteractive)return false;
  const auto hasTrust=hasTrustDialogAccepted();
  return !hasTrust;
}
} // namespace batbox::instructions::hooks::runtime
