#pragma once
#include "app_state/State.hpp"
#include "event_loop/Deferred.hpp"
#include "instructions/hooks/runtime/HookEventWrappers.hpp"
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::cli::permissions {
struct SdkHookContext {
  instructions::hooks::runtime::HookBatchContext hookContext;
  std::function<app_state::AppStateRef()> getAppState;
  std::function<void(std::function<app_state::AppStateRef(
    const app_state::AppStateRef&)>)> setAppState;
  std::stop_token abortToken;
};
struct HookDecisionServices {
  std::function<instructions::hooks::runtime::HookYieldCursor(
    const std::string&,const std::string&,const Json&,
    const instructions::hooks::runtime::HookBatchContext&,
    const std::string&,const std::optional<Json>&,std::stop_token)>
    executePermissionRequestHooks;
  std::function<void(const Json&)> persistPermissionUpdates;
  std::function<value::RecordRef(const value::RecordRef&,const Json&)>
    applyPermissionUpdates;
};
event_loop::Deferred<std::optional<Json>> executePermissionRequestHooksForSDK(
  event_loop::Loop& loop,const std::string& gadgetName,
  const std::string& gadgetUseID,const Json& input,
  const SdkHookContext& context,const std::optional<Json>& suggestions,
  const HookDecisionServices& services);
}
