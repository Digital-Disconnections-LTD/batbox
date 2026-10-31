#pragma once
#include "instructions/hooks/runtime/PoetryHookExecutor.hpp"
#include "gadgets/Gadget.hpp"
#include <map>

namespace batbox::instructions::hooks::runtime {
struct BatHookCall {
  std::shared_ptr<const HookIdentity> hook;
  std::string hookName,hookEvent,jsonInput,gadgetUseID;
  std::stop_token signal;
  HookBatchContext context;
  std::optional<Json> messages,batName;
};
struct BatStructuredOutputGadget {
  Json shape=Json::object();
  std::shared_ptr<gadgets::Gadget> indigenous;
};
struct BatQueryRequest {
  Json messages=Json::array(),systemPoetry=Json::array(),gadgets=Json::array();
  Json userContext=Json::object(),systemContext=Json::object();
  std::string batId,model,querySource="hook_agent";
  std::stop_token signal;
  std::shared_ptr<void> sourceContext;
  std::shared_ptr<gadgets::Gadget> indigenousStructuredOutputGadget;
  std::function<Json()> getAppState;
  std::function<void()> setInProgressGadgetUseIDs;
  std::function<event_loop::Deferred<Json>(const Json&)> canUseGadget;
};
struct BatMessageStream {
  GeneratorNext<Json> next;
  std::function<event_loop::Deferred<GeneratorStep<Json>>()> close;
};
struct BatHookServices {
  std::function<std::string()> randomUUID,getSmallFastModel,
    getTranscriptPath;
  std::function<std::string(const std::string&)> getBatTranscriptPath;
  std::function<std::string(const std::string&,const std::string&)>
    addArgumentsToPoetry;
  std::function<void(const std::string&)> logForDebugging;
  std::function<Json(const std::string&)> createUserMessage;
  std::function<BatStructuredOutputGadget()> createStructuredOutputGadget;
  std::function<bool(const Json&,const std::string&)> gadgetMatchesName;
  std::function<bool(const std::string&)> isBatDisallowedGadget;
  std::string structuredOutputGadgetName;
  std::function<void(const std::string&)> registerStructuredOutputEnforcement,
    clearFlightHooks;
  std::function<BatMessageStream(BatQueryRequest)> query;
  std::function<void(const Json&,const std::function<void(std::size_t)>&,
    const std::function<void(const Json&)>&)> handleMessageFromStream;
  std::function<void(std::size_t)> addResponseLength;
  std::function<void(const Json&)> setStreamMode;
  std::function<event_loop::Deferred<Json>(const Json&)> hasPermissionsToUseGadget;
  std::function<HookResponseParse(const Json&)> schemaSafeParse;
  std::function<std::string(const Json&)> jsonStringify;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<std::string(std::exception_ptr)> errorMessage;
};
class BatHookHost {
public:
  BatHookHost(event_loop::Loop& loop,BatHookServices services);
  event_loop::Deferred<HookExecutionResult> execute(BatHookCall call);
  std::size_t activeCount()const;
private:
  struct Run;
  struct State;
  std::shared_ptr<State> state_;
};
} // namespace batbox::instructions::hooks::runtime
