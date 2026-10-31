#pragma once
#include "instructions/hooks/runtime/HookEventWrappers.hpp"
#include "instructions/hooks/runtime/StopFailureHooks.hpp"

namespace batbox::instructions::hooks::runtime {
struct GatedGadgetServices {
  HookEventWrapperServices wrapper;
  std::function<std::string()> getFlightId;
  std::function<bool(const std::string&,const std::optional<Json>&,
    const std::string&)> hasHookForEvent;
};
GatedGadgetServices bindGatedGadgetServices(HookEventWrapperServices wrapper,
  std::function<std::string()> getFlightId,HookPresenceServices presence);
struct PostGadgetFailureCall {
  std::string gadgetName,gadgetUseID,error;
  Json gadgetInput;
  HookBatchContext gadgetUseContext;
  std::optional<bool> isInterrupt;
  std::optional<std::string> permissionMode;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
};
struct PermissionDeniedCall {
  std::string gadgetName,gadgetUseID,reason;
  Json gadgetInput;
  HookBatchContext gadgetUseContext;
  std::optional<std::string> permissionMode;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
};
HookEventGenerator executePostGadgetUseFailureHooks(event_loop::Loop& loop,
  PostGadgetFailureCall call,GatedGadgetServices services);
HookEventGenerator executePermissionDeniedHooks(event_loop::Loop& loop,
  PermissionDeniedCall call,GatedGadgetServices services);
} // namespace batbox::instructions::hooks::runtime
