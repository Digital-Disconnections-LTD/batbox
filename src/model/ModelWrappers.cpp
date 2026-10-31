#include "model/ModelWrappers.hpp"
#include "utils/envUtils.hpp"
#include <cmath>

namespace batbox::model {

MessageRef queryModelWithoutStreaming(const StreamFactory& streamFactory,
                                      const std::function<bool()>& signalAborted) {
  MessageRef assistantMessage;
  auto next = streamFactory();
  while (auto message = next()) {
    if ((*message)->is_object() && value::property(**message, "type") == "assistant")
      assistantMessage = *message;
  }
  if (!assistantMessage) {
    if (signalAborted()) throw UserAbortError();
    throw std::runtime_error("No assistant message found");
  }
  return assistantMessage;
}

NextMessage queryModelWithStreaming(const StreamFactory& streamFactory) {
  return [streamFactory, source = std::optional<NextMessage>{}]() mutable -> std::optional<MessageRef> {
    if (!source) source = streamFactory();
    return (*source)();
  };
}

double getNonstreamingFallbackTimeoutMs(const Json& environment) {
  const Json& value = value::property(environment, "API_TIMEOUT_MS");
  const double override = value::parseInt10(value.is_string() ? value.get<std::string>() : "");
  if (std::isfinite(override) && override != 0) return override;
  const Json& remote = value::property(environment, "BATBOX_REMOTE");
  return isEnvTruthy(remote) ? 120000 : 300000;
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
