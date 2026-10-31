#pragma once
#include "instructions/hooks/runtime/HookBlockingMessages.hpp"
#include "instructions/hooks/runtime/HookSchema.hpp"
#include "instructions/hooks/runtime/BaseHookInput.hpp"
#include <stop_token>

namespace batbox::instructions::hooks::runtime {
struct ElicitationResponse {
  std::string action;
  std::optional<Json> content;
  bool contentOwnProperty=true;
};
struct ElicitationParsed {
  std::optional<ElicitationResponse> response;
  std::optional<HookBlockingError> blockingError;
};
ElicitationParsed parseElicitationHookOutput(const HookResult& result,
  const std::string& expectedEventName,
  const std::function<SchemaResult(const Json&)>& schemaSafeParse);
struct ElicitationRequest {
  std::string serverName,message;
  std::optional<Json> requestedSchema;
  std::optional<std::string> permissionMode,mode,url,elicitationId;
  std::optional<std::stop_token> signal;
  std::optional<double> timeoutMs;
};
struct ElicitationResultRequest {
  std::string serverName,action;
  std::optional<Json> content;
  std::optional<std::string> permissionMode,mode,elicitationId;
  std::optional<std::stop_token> signal;
  std::optional<double> timeoutMs;
};
struct ElicitationHookOutput {
  std::optional<ElicitationResponse> response;
  std::optional<HookBlockingError> blockingError;
};
struct ElicitationHookServices {
  std::function<BaseHookInput(std::optional<std::string>)> createBaseHookInput;
  std::function<event_loop::Deferred<std::vector<HookResult>>(
    HookDispatch,std::optional<std::stop_token>)> executeHooksOutsideRepl;
  std::function<SchemaResult(const Json&)> schemaSafeParse;
  double defaultTimeoutMs=0;
};
event_loop::Deferred<ElicitationHookOutput> executeElicitationHooks(
  event_loop::Loop& loop,const ElicitationHookServices& services,
  const ElicitationRequest& request);
event_loop::Deferred<ElicitationHookOutput> executeElicitationResultHooks(
  event_loop::Loop& loop,const ElicitationHookServices& services,
  const ElicitationResultRequest& request);
} // namespace batbox::instructions::hooks::runtime
