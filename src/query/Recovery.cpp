#include "query/Recovery.hpp"
#include <stdexcept>

namespace batbox::query {

void yieldMissingGadgetResultBlocks(const Json& assistantMessages, const std::string& errorMessage,
                                  const CreateUserMessage& createUserMessage,
                                  const std::function<void(const Json&)>& yield) {
  if (!createUserMessage || !yield)
    throw std::invalid_argument("Missing tool result callbacks are required");
  for (const Json& assistantMessage : assistantMessages) {
    const Json& message = value::property(assistantMessage, "message");
    const Json& content = value::property(message, "content");
    if (!content.is_array()) throw std::invalid_argument("Assistant message content must be an array");
    for (const Json& gadgetUse : content) {
      if (value::property(gadgetUse, "type") != "tool_use") continue;
      Json resultBlock={{"type", "tool_result"}, {"content", errorMessage},
                        {"is_error", true}, {"tool_use_id", value::property(gadgetUse, "id")}};
      Json args={{"content", Json::array({resultBlock})}, {"toolUseResult", errorMessage},
                 {"sourceToolAssistantUUID", value::property(assistantMessage, "uuid")}};
      yield(createUserMessage(args));
    }
  }
}

bool isWithheldMaxOutputTokens(const Json& message) {
  return value::property(message, "type") == "assistant" &&
         value::property(message, "apiError") == "max_output_tokens";
}

} // namespace batbox::query
