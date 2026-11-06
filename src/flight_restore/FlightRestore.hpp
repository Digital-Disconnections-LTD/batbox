#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::flight_restore {
// Indigenous adapter for TodoListSchema().safeParse: strip unknown item keys and
// return an empty list when any item fails validation.
Json parseTodoList(const Json& value);

struct Services {
  std::function<Json(const Json&)> parseTodoList;
};

Json extractTodosFromTranscript(const Json& messages,Services& services);
std::optional<Json> computeStandaloneBatContext(
  const std::optional<std::string>& batName,
  const std::optional<std::string>& batColor);
} // namespace batbox::flight_restore
