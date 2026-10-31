#pragma once
#include "instructions/hooks/runtime/HookMatching.hpp"
#include "settings/PermissionRules.hpp"
#include "gadgets/Gadget.hpp"

namespace batbox::instructions::hooks::runtime {
std::function<event_loop::Deferred<std::optional<IfConditionMatcher>>(const Json&)>
indigenousPrepareIfConditionMatcher(event_loop::Loop& loop,gadgets::Gadgets gadgets,
  settings::PermissionRuleServices rules);
} // namespace batbox::instructions::hooks::runtime
