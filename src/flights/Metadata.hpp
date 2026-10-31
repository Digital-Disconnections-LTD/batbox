#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
namespace batbox::flights {
std::string unescapeJsonString(const std::string& raw);
std::optional<std::string> extractJsonStringField(const std::string& text,const std::string& key);
std::optional<std::string> extractLastJsonStringField(const std::string& text,const std::string& key);
std::string readFileTailSync(const std::string& file);
void appendEntryToFile(const std::string& file,const Json& entry);
}
