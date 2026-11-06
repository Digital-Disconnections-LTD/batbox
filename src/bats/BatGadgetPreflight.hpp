#pragma once
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <functional>
#include <string>

namespace batbox::bats {
struct BatGadgetPreflightServices {
  std::function<std::int64_t()> nowMillis;
  std::function<void(std::int64_t)> sleepMillis;
  std::function<Json()> getAppState;
  std::function<bool()> isForkSubagentEnabled;
  std::function<bool(const Json&)> isInForkChild;
  std::function<Json(const Json&,const Json&,const std::string&)> filterDeniedBats;
  std::function<Json(const Json&,const std::string&,const std::string&)> getDenyRuleForBat;
  std::function<void(const std::string&,const Json&)> setBatColor;
  Json forkBat,generalPurposeBat;
  std::string batGadgetName="Agent";
};
struct BatGadgetPreflightResult {
  Json selectedBat=Json();
  Json modelParam=Json();
  Json effectiveType=Json();
  Json initialAppState=Json();
  Json permissionMode=Json();
  bool isForkPath=false;
  std::int64_t startTime=0;
};
bool hasRequiredMcpServers(const Json& bat,const Json& availableServers);
Json filterBatsByMcpRequirements(const Json& bats,const Json& availableServers);
BatGadgetPreflightResult preflightBatGadget(const Json& input,const Json& options,
  const Json& messages,BatGadgetPreflightServices& services);
} // namespace batbox::bats
