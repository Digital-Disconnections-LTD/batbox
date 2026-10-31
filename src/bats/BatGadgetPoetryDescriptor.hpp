#pragma once
#include "gadgets/Gadget.hpp"

namespace batbox::bats {
struct BatGadgetPoetryDependencies {
  std::function<Json(const Json& bats,const Json& permissionContext,
    const std::string& gadgetName)> filterDeniedBats;
  std::function<bool(const std::string&)> feature;
  std::function<bool()> coordinatorEnvironmentEnabled;
  std::function<std::future<std::string>(const Json& bats,bool isCoordinator,
    const Json& allowedBatTypes)> getPoetry;
  std::string batGadgetName="Agent";
};
// Filter the available bats before invoking the supplied poetry declaration.
std::future<std::string> getBatGadgetPoetry(const gadgets::PoetryContext& context,
  const BatGadgetPoetryDependencies& dependencies);
} // namespace batbox::bats
