#pragma once
#include "instructions/hooks/runtime/ProcessHookJson.hpp"
#include "event_loop/Deferred.hpp"
#include <memory>
#include <stop_token>
#include <variant>

namespace batbox::instructions::hooks::runtime {
struct HookCallbackContext {
  std::function<Json()> getAppState;
  std::function<void(const Json&)> updateAttributionState;
};
using HookCallbackReply=std::variant<Json,event_loop::Deferred<Json>>;
struct HookCallback {
  Json fields=Json::object();
  std::function<HookCallbackReply(const Json&,const std::string&,
    std::stop_token,std::optional<std::size_t>,
    const std::optional<HookCallbackContext>&)> callback;
};
struct HookCallbackCall {
  std::string gadgetUseID,hookEvent;
  Json hookInput=Json::object();
  std::stop_token signal;
  std::optional<std::size_t> hookIndex;
  std::optional<HookCallbackContext> gadgetUseContext;
  std::shared_ptr<const HookCallback> hook;
};
struct HookCallbackResult {
  Json fields=Json::object();
  std::vector<std::string> ownUndefinedKeys;
  std::shared_ptr<const HookCallback> hook;
};
event_loop::Deferred<HookCallbackResult> executeHookCallback(
  event_loop::Loop& loop,const HookCallbackCall& call,
  const ProcessHookJsonServices& processor);
} // namespace batbox::instructions::hooks::runtime
