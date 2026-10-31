#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::shell_quote {
using EnvLookup=std::function<std::optional<Json>(const std::string&)>;
Json parse(const std::string& command,const EnvLookup& environment={},
  const std::string& escape="\\");
std::string quote(const std::vector<Json>& arguments);
} // namespace batbox::shell_quote
