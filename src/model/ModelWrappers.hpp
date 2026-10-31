#pragma once

#include "runtime/JsonValue.hpp"
#include "transport/Errors.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace batbox::model {

using MessageRef = std::shared_ptr<Json>;
using NextMessage = std::function<std::optional<MessageRef>()>;
using StreamFactory = std::function<NextMessage()>;

using UserAbortError = transport::APIUserAbortError;

// Consume the stream factory and return its final assistant message.
MessageRef queryModelWithoutStreaming(const StreamFactory& streamFactory,
                                      const std::function<bool()>& signalAborted);
NextMessage queryModelWithStreaming(const StreamFactory& streamFactory);

double getNonstreamingFallbackTimeoutMs(const Json& environment);
std::optional<std::string> getPreviousRequestIdFromMessages(const Json& messages);

} // namespace batbox::model
