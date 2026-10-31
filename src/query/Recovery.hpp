#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::query {

using CreateUserMessage = std::function<Json(const Json& args)>;

void yieldMissingGadgetResultBlocks(const Json& assistantMessages, const std::string& errorMessage,
                                  const CreateUserMessage& createUserMessage,
                                  const std::function<void(const Json&)>& yield);
bool isWithheldMaxOutputTokens(const Json& message);

} // namespace batbox::query
