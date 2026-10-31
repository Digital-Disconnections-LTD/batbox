#pragma once
#include "bootstrap/BootstrapState.hpp"
#include "instructions/hooks/runtime/HookMatching.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::bootstrap {
struct BootstrapBindings {
  std::function<void(const std::optional<std::string>&)>
    setStashedBatboxMdContent;
  std::function<void(const std::string&,const std::string&,
    const std::string&,const std::optional<std::string>&)> addInvokedSkill;
  std::function<void(const std::string&)> clearInvokedSkillsForBat;
  std::function<Json(const Json&)> getInvokedSkillsForBat;
  std::function<void(const std::string&,double)> addSlowOperation;
  std::function<std::optional<std::vector<
    instructions::hooks::runtime::HookMatcher>>(
      const std::string&)> getRegisteredHooksForEvent;
};
BootstrapBindings bindBootstrapState(std::shared_ptr<BootstrapState> state,
  std::function<double()> nowMs,bool developerUser);
}
