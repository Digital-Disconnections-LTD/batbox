#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::messages {
// Services remain explicit. Callbacks are required only when execution
// reaches their corresponding call site.
struct Services {
  std::function<std::string()> randomUUID;
  std::function<std::string()> nowISO;
  std::function<bool()> strictGadgetResultPairing;
  std::function<bool(const std::string&)> featureGate;
  std::function<void(const std::string&)> logError;
};

std::string deriveUUID(const std::string& parentUUID, std::size_t index);
Json createUserMessage(const Json& args, Services& services);
Json createAssistantMessage(const Json& args, Services& services);
Json createAssistantAPIErrorMessage(const Json& args, Services& services);
Json prepareUserContent(const Json& args);
Json normalizeMessages(const Json& messages, Services& services);
Json ensureGadgetResultPairing(const Json& messages, Services& services);
} // namespace batbox::messages
