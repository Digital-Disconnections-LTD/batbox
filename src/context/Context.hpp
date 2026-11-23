#pragma once
#include "instructions/project/ProjectDiscovery.hpp"
#include "runtime/IndigenousTime.hpp"
#include "runtime/JsonValue.hpp"
#include "utils/envUtils.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
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
inline std::string getLocalISODate(const std::optional<std::string>& overrideDate={}){
  if(overrideDate&&!overrideDate->empty())return *overrideDate;
  const auto now=std::chrono::system_clock::now();
  const auto value=std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  if(!runtime::localtimeIndigenous(value,local))
    throw std::runtime_error("Local date conversion failed");
  char date[32];
  std::snprintf(date,sizeof(date),"%04d-%02d-%02d",local.tm_year+1900,
    local.tm_mon+1,local.tm_mday);
  return date;
}
inline bool shouldIncludeGitInstructions(const Json& environment,
  const Json& initialSettings){
  const auto& env=value::property(environment,"BATBOX_DISABLE_GIT_INSTRUCTIONS");
  if(isEnvTruthy(env))return false;
  if(isEnvDefinedFalsy(env))return true;
  const auto& setting=value::property(initialSettings,"includeGitInstructions");
  return setting.is_null()?true:value::truthy(setting);
}
}
