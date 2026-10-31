#pragma once
#include "runtime/JsonValue.hpp"
#include <vector>

namespace batbox::cli::headless {
void removeInterruptedMessage(std::vector<Json>& messages,
                              const Json& interruptedUserMessage);
}
