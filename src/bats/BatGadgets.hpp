#pragma once
#include "gadgets/Gadget.hpp"
#include <optional>
#include <unordered_set>

namespace batbox::bats {
struct BatGadgetFilterImports {
  std::unordered_set<std::string> allBatDisallowed,customBatDisallowed,
    asyncBatAllowed,inProcessPartnerAllowed;
  std::string exitPlanModeGadgetName,batGadgetName;
  std::function<bool(const gadgets::Gadget&,const std::string&)> gadgetMatchesName;
  std::function<bool()> isBatSwarmsEnabled,isInProcessPartner;
  std::function<Json(const std::string& spec)> permissionRuleValueFromString;
};
struct ResolvedBatGadgets {
  bool hasWildcard=false;
  std::vector<std::string> validGadgets,invalidGadgets;
  gadgets::Gadgets resolvedGadgets;
  std::optional<std::vector<std::string>> allowedBatTypes;
};
gadgets::Gadgets filterGadgetsForBat(const gadgets::Gadgets& available,bool isBuiltIn,
  bool isAsync,const std::optional<std::string>& permissionMode,
  BatGadgetFilterImports& imports);
ResolvedBatGadgets resolveBatGadgets(const Json& batDefinition,
  const gadgets::Gadgets& available,bool isAsync,bool isMainThread,
  BatGadgetFilterImports& imports);
} // namespace batbox::bats
