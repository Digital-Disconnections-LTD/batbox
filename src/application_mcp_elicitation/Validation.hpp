#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>
#include <vector>

namespace batbox::application_mcp_elicitation {
struct Option {Json value;std::string label;};
// Indigenous elicitation validation helpers. Values are
// returned separately from titles so the wire response never contains labels.
std::vector<Option> optionsFor(const Json& schema);
std::optional<Json> validateInput(const std::string& raw,const Json& schema,
  bool required,std::string& error);
}
