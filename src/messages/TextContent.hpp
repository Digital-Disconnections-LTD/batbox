#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>

namespace batbox::messages {
std::string extractTextContent(const Json& blocks,const std::string& separator="");
std::optional<std::string> getContentText(const Json& content);
}
