#pragma once
#include "bats/BatGadgetLaunch.hpp"
#include "bats/BatMissionRegistration.hpp"

namespace batbox::bats {
struct BatGadgetAsyncServices {
  BatRegistrationServices* registration=nullptr;
  BatGadgetWorktreeServices worktree;
  std::function<std::optional<std::string>()> getParentFlightId;
  std::function<bool(const Json&)> isBuiltInBat;
  std::function<bool()> isForkSubagentEnabled,getSdkBatProgressSummariesEnabled;
  std::function<void(const Json& batContext,const std::function<void()>&)>
    runWithBatContext;
  std::function<void(const std::string& cwd,const std::function<void()>&)>
    runWithCwdOverride;
  // Must schedule and retain the lifecycle after the call returns.
  std::function<void(AsyncLifecycleParams)> launchLifecycle;
  std::function<void(const RunBatParams&,const query::Yield&)> runBat;
  std::function<std::string(const std::string&)> getMissionOutputPath;
  std::string fileReadGadgetName="Read",bashGadgetName="Bash";
};
Json launchBatGadgetAsync(const Json& input,const Json& assistantMessage,
  const BatContext& context,BatGadgetLaunchPlan plan,
  BatGadgetAsyncServices& services);
} // namespace batbox::bats
