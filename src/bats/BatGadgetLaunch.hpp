#pragma once
#include "bats/BatGadgetPoetry.hpp"
#include "bats/BatGadgetWorktree.hpp"
#include "bats/RunBat.hpp"

namespace batbox::bats {
struct BatGadgetLaunchServices {
  BatGadgetPoetryServices poetry;
  std::function<std::string(const Json&,const Json&,const Json&,const Json&)> getBatModel;
  std::function<bool(const Json&)> isBuiltInBat;
  std::function<bool(const std::string&)> feature;
  std::function<bool()> coordinatorEnvironmentEnabled,isForkSubagentEnabled,
    isBackgroundMissionsDisabled;
  std::function<bool()> isProactiveActive; // optional proactive capability
  std::function<gadgets::Gadgets(const Json&,const Json&)> assembleGadgetPool;
  std::function<std::string()> createBatId,getCwd;
  std::function<Json(const std::string&)> createBatWorktree;
  std::function<std::string(const std::string&,const std::string&)> buildWorktreeNotice;
  std::function<Json(const Json&)> createUserMessage,asSystemPoetry;
  std::function<std::string(const std::string&,bool)> getQuerySourceForBat;
};
struct BatGadgetLaunchPlan {
  RunBatParams runBatParams;
  BatGadgetWorktreeState worktree;
  Json metadata=Json::object();
  std::optional<std::string> cwdOverridePath;
  std::function<void(const std::function<Json(const Json&)>&)> rootSetAppState;
  std::string earlyBatId,resolvedBatModel;
  bool shouldRunAsync=false,isCoordinator=false;
};
BatGadgetLaunchPlan prepareBatGadgetLaunch(const Json& input,
  const BatGadgetPreflightResult& preflight,const Json& assistantMessage,
  const BatContext& context,
  const std::function<std::future<Json>(const Json&)>& canUseGadget,
  BatGadgetLaunchServices& services);
} // namespace batbox::bats
