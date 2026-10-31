#pragma once
#include "instructions/hooks/runtime/HookBatchStart.hpp"
#include "instructions/hooks/runtime/FunctionHook.hpp"
#include "instructions/hooks/runtime/LazyHookJsonInput.hpp"
#include "instructions/hooks/runtime/HookOutputParser.hpp"

namespace batbox::instructions::hooks::runtime {
// One async generator per matching hook in executeHooks. Its body starts on
// the first next() call.
struct HookPerHookInput {
  HookBatchMatch match;
  std::size_t hookIndex=0;
  Json hookInput=Json::object();
  std::string hookName,hookEvent,gadgetUseID;
  std::optional<Json> messages;
  std::optional<std::stop_token> signal;
  std::optional<HookBatchContext> gadgetUseContext;
  double timeoutMs=0;
  bool forceSyncExecution=false;
  BoundHookPoetry boundRequestPoetry;
};
struct HookCommandCall {
  Json hook=Json::object();
  std::string hookEvent,hookName,jsonInput,hookId;
  std::stop_token signal;
  std::size_t hookIndex=0;
  std::optional<std::string> pluginRoot,pluginId,skillRoot;
  bool forceSyncExecution=false;
  BoundHookPoetry boundRequestPoetry;
};
struct HookModelCall {
  std::shared_ptr<const HookIdentity> hookIdentity;
  Json hook=Json::object();
  std::string hookName,hookEvent,jsonInput,gadgetUseID;
  std::stop_token signal;
  HookBatchContext gadgetUseContext;
  std::optional<Json> messages,batType;
};
struct HookHttpCall {
  Json hook=Json::object();
  std::string hookEvent,jsonInput;
  std::optional<std::stop_token> signal;
};
struct HookPerHookServices {
  ProcessHookJsonServices processJson;
  FunctionHookHost* functionHost=nullptr;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::shared_ptr<LazyHookJsonInput> jsonInput;
  std::function<std::string()> randomUUID;
  std::function<double()> nowMillis;
  std::function<std::string(const HookBatchMatch&)> getHookDisplayText;
  std::function<event_loop::Deferred<CommandOutcome>(HookCommandCall)> execCommandHook;
  std::function<event_loop::Deferred<HookExecutionResult>(HookModelCall)>
    execPoetryHook,execBatHook;
  std::function<event_loop::Deferred<HttpOutcome>(HookHttpCall)> execHttpHook;
  std::function<HookParseResult(const std::string&)> parseHookOutput;
  std::function<HookParseResult(const std::string&)> parseHttpHookOutput;
  std::function<void(const std::string&,const std::string&,
    const std::string&)> emitHookStarted;
  std::function<void(const Json&)> emitHookResponse;
  std::function<void(const Json&,const std::vector<std::string>&)>
    emitHookResponseWithUndefined;
  std::function<std::string(const std::string&)> bold;
  std::function<std::string(std::exception_ptr)> errorMessage;
};
GeneratorNext<HookExecutionResult> makeHookPerHookGenerator(
  event_loop::Loop& loop,HookPerHookInput input,HookPerHookServices services);
GeneratorNext<HookExecutionResult> makeHookCommandGenerator(
  event_loop::Loop& loop,HookPerHookInput input,HookPerHookServices services);
GeneratorNext<HookExecutionResult> makeHookModelGenerator(
  event_loop::Loop& loop,HookPerHookInput input,HookPerHookServices services);
GeneratorNext<HookExecutionResult> makeHookHttpGenerator(
  event_loop::Loop& loop,HookPerHookInput input,HookPerHookServices services);
} // namespace batbox::instructions::hooks::runtime
