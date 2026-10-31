#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
namespace batbox::cli {
using NdjsonStringifier=std::function<std::optional<std::string>(const Json&)>;
std::string escapeJsLineTerminators(const std::string& json);
std::string ndjsonSafeStringify(const Json& value,const NdjsonStringifier& stringify);
}
