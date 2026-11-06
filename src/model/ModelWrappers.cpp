#include "model/ModelWrappers.hpp"
#include <cmath>

namespace batbox::model {

double getNonstreamingFallbackTimeoutMs(const Json& environment) {
  const Json& value = value::property(environment, "API_TIMEOUT_MS");
  const double override = value::parseInt10(value.is_string() ? value.get<std::string>() : "");
  // 0 means no limit.
  if (std::isfinite(override) && override >= 0) return override;
  return 0;
}

std::optional<std::string> getPreviousRequestIdFromMessages(const Json& messages) {
  for (std::size_t i = messages.size(); i > 0; --i) {
    const Json& message = messages[i - 1];
    if (value::property(message, "type") == "assistant") {
      const Json& requestId = value::property(message, "requestId");
      if (value::truthy(requestId)) return requestId.get<std::string>();
    }
  }
  return std::nullopt;
}

} // namespace batbox::model
