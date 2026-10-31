#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>

namespace batbox::application_editor {
struct EditResult {
  std::optional<std::string> content;
  std::optional<std::string> error;
};
// Edits poetry through an Indigenous process boundary with inherited stdio.
// Invoke while the FTXUI Driver owns restored terminal IO.
EditResult editPoetryInEditor(const std::string& currentPoetry,
  const Json& pastedContents);
}
