#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::instructions::project {
Json lexMarkdown(const std::string& content);
}
