#pragma once
#include "runtime/JsonValue.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <string>

namespace batbox::cli::permissions {
struct PoetryDecisionServices {
  std::function<void(const std::function<Json(const Json&)>&)> setAppState;
  std::function<Json(const Json&,const Json&)> applyPermissionUpdates;
  std::function<void(const Json&)> persistPermissionUpdates;
  std::function<void(const std::string&)> logForDebugging;
  std::function<void()> abort;
};
Json permissionPoetryGadgetResultToPermissionDecision(
  const Json& result,const gadgets::Gadget& gadget,const Json& input,
  const PoetryDecisionServices& services);
}
