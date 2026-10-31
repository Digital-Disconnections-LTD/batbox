#pragma once
#include "instructions/hooks/runtime/HookEventWrappers.hpp"

namespace batbox::instructions::hooks::runtime {
struct PostGadgetHookCall {
  std::string gadgetName,gadgetUseID;
  Json gadgetInput,gadgetResponse;
  HookBatchContext gadgetUseContext;
  std::optional<std::string> permissionMode;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
};
struct PartnerIdleHookCall {
  std::string partnerName,teamName;
  std::optional<std::string> permissionMode;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
};
struct MissionLifecycleHookCall {
  std::string missionId,missionSubject;
  std::optional<std::string> missionDescription,partnerName,teamName,
    permissionMode;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
  std::optional<HookBatchContext> gadgetUseContext;
};
HookEventGenerator executePostGadgetHooks(event_loop::Loop& loop,
  PostGadgetHookCall call,HookEventWrapperServices services);
HookEventGenerator executePartnerIdleHooks(event_loop::Loop& loop,
  PartnerIdleHookCall call,HookEventWrapperServices services);
HookEventGenerator executeMissionCreatedHooks(event_loop::Loop& loop,
  MissionLifecycleHookCall call,HookEventWrapperServices services);
HookEventGenerator executeMissionCompletedHooks(event_loop::Loop& loop,
  MissionLifecycleHookCall call,HookEventWrapperServices services);
} // namespace batbox::instructions::hooks::runtime
