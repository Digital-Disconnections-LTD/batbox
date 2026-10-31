#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::bats {
struct BatResultMetadata {
  std::string poetry,resolvedBatModel,batType;
  bool isBuiltInBat=false,isAsync=false;
  std::int64_t startTime=0;
};
struct BatResultImports {
  std::function<Json(const Json&)> getLastAssistantMessage;
  std::function<double(const Json& usage)> getTokenCountFromUsage;
  std::function<std::string(const Json& content,const std::string& separator)>
    extractTextContent;
  std::function<std::int64_t()> nowMillis;
};
int countGadgetUses(const Json& messages);
Json finalizeBatGadget(const Json& messages,const std::string& batId,
  const BatResultMetadata& metadata,BatResultImports& imports);
std::optional<std::string> getLastGadgetUseName(const Json& message);
std::optional<std::string> extractPartialResult(const Json& messages,
  BatResultImports& imports);
} // namespace batbox::bats
