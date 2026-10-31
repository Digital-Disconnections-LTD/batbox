#pragma once
#include "instructions/hooks/runtime/HookEventWrappers.hpp"

namespace batbox::instructions::hooks::runtime {
using RequestPoetryFactory=std::function<BoundHookPoetry(
  const std::string&,const std::optional<Json>&)>;
struct PoetryGadgetServices {
  HookEventWrapperServices wrapper;
  std::function<std::string()> getFlightId;
  std::function<bool(const std::string&,const std::optional<Json>&,
    const std::string&)> hasHookForEvent;
  std::function<void(const std::string&,const std::optional<Json>&)>
    logForDebugging;
  std::function<HookYieldCursor(ExecuteHooksInput,RequestPoetryFactory)>
    executeWithPoetry;
};
PoetryGadgetServices bindPoetryGadgetServices(event_loop::Loop& loop,
  HookEventWrapperServices wrapper,std::function<std::string()> getFlightId,
  std::function<bool(const std::string&,const std::optional<Json>&,
    const std::string&)> hasHookForEvent,
  std::function<void(const std::string&,const std::optional<Json>&)>
    logForDebugging,ExecuteHooksServices executor);
struct PreGadgetHookCall {
  std::string gadgetName,gadgetUseID;
  Json gadgetInput;
  HookBatchContext gadgetUseContext;
  std::optional<std::string> permissionMode;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
  RequestPoetryFactory requestPoetry;
  std::optional<Json> gadgetInputSummary;
};
struct PermissionRequestHookCall {
  std::string gadgetName,gadgetUseID;
  Json gadgetInput;
  HookBatchContext gadgetUseContext;
  std::optional<std::string> permissionMode;
  std::optional<Json> permissionSuggestions;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
  RequestPoetryFactory requestPoetry;
  std::optional<Json> gadgetInputSummary;
};
HookEventGenerator executePreGadgetHooks(event_loop::Loop& loop,
  PreGadgetHookCall call,PoetryGadgetServices services);
HookEventGenerator executePermissionRequestHooks(event_loop::Loop& loop,
  PermissionRequestHookCall call,PoetryGadgetServices services);
} // namespace batbox::instructions::hooks::runtime
