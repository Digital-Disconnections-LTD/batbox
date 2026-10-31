#pragma once
#include "runtime/JsonValue.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::cli::permissions {
std::optional<std::string> serializeDecisionReason(
  const Json& reason,const std::function<bool(const std::string&)>& feature);
Json buildRequiresActionDetails(const gadgets::Gadget& gadget,const Json& input,
  const std::string& gadgetUseID,const std::string& requestId);
}
