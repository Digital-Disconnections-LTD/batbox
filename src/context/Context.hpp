#pragma once
#include "instructions/project/ProjectDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::context {
using MemoryFiles=std::vector<instructions::project::MemoryFileInfo>;
struct ContextInputs {
  Json environment=Json::object();
  bool testEnvironment=false;
  std::function<void(std::exception_ptr)> logError;
  std::function<bool()> includeGitInstructions,stashBreakerFeature,bareMode;
  std::function<std::vector<std::string>()> additionalInstructionDirectories;
  const instructions::project::DiscoveryServices* projectServices=nullptr;
  std::function<void(const std::optional<std::string>&)> setStashedBatboxMdContent;
  std::function<std::string()> localISODate;
};
std::string getLocalISODate(const std::optional<std::string>& overrideDate={});
bool shouldIncludeGitInstructions(const Json& environment,
  const Json& initialSettings);
}
