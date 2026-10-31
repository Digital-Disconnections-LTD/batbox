#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::message_view {
struct MetadataPart {std::string text;int minWidth=0;};
struct MetadataImports {
  std::function<std::string(const std::string&)> formatLocalTime;
  std::function<int(const std::string&)> stringWidth;
};
std::optional<MetadataPart> timestampMetadata(const Json& message,bool transcript,
  const MetadataImports& imports);
std::optional<MetadataPart> modelMetadata(const Json& message,bool transcript,
  const MetadataImports& imports);
} // namespace batbox::interactive::message_view
