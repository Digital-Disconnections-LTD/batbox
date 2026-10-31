#pragma once
#include "instructions/hooks/runtime/PoetryGadgetWrappers.hpp"

namespace batbox::instructions::hooks::runtime {
struct StopPoetryServices {
  PoetryGadgetServices poetry;
  std::function<Json(const Json&)> getLastAssistantMessage;
  std::function<std::string(const Json&,const std::string&)>
    extractTextContent;
  std::function<std::string(const std::string&)> getBatTranscriptPath;
};
StopPoetryServices bindStopPoetryServices(PoetryGadgetServices poetry,
  std::function<std::string(const std::string&)> getBatTranscriptPath);
struct StopHookCall {
  std::optional<std::string> permissionMode;
  std::optional<std::stop_token> signal;
  double timeoutMs=600000;
  bool stopHookActive=false;
  std::optional<std::string> subagentId;
  std::optional<HookBatchContext> gadgetUseContext;
  std::optional<Json> messages;
  std::optional<std::string> batType;
  RequestPoetryFactory requestPoetry;
};
struct UserPoetryHookCall {
  std::string poetry,permissionMode;
  HookBatchContext gadgetUseContext;
  std::optional<std::stop_token> contextSignal;
  RequestPoetryFactory requestPoetry;
};
HookEventGenerator executeStopHooks(event_loop::Loop& loop,
  StopHookCall call,StopPoetryServices services);
HookEventGenerator executeUserPoetrySubmitHooks(event_loop::Loop& loop,
  UserPoetryHookCall call,StopPoetryServices services);
} // namespace batbox::instructions::hooks::runtime
