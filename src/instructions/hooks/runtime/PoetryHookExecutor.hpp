#pragma once
#include "instructions/hooks/runtime/HookPerHook.hpp"
#include <map>

namespace batbox::instructions::hooks::runtime {
struct PoetryHookCall {
  std::shared_ptr<const HookIdentity> hook;
  std::string hookName,hookEvent,jsonInput,gadgetUseID;
  std::stop_token signal;
  HookBatchContext context;
  std::optional<Json> messages;
};
struct PoetryQueryRequest {
  Json messages=Json::array();
  Json systemPoetry=Json::array();
  Json thinkingConfig={{"type","disabled"}};
  std::optional<Json> gadgets;
  std::stop_token signal;
  Json options=Json::object();
  std::vector<std::string> ownUndefinedKeys;
  std::shared_ptr<void> sourceContext;
  std::function<event_loop::Deferred<Json>()> getGadgetPermissionContext;
};
struct HookResponseParse {
  bool success=false;
  Json data=Json::object();
  std::string errorMessage;
};
struct PoetryHookServices {
  std::function<std::string()> randomUUID,getSmallFastModel;
  std::function<std::string(const std::string&,const std::string&)>
    addArgumentsToPoetry;
  std::function<void(const std::string&)> logForDebugging;
  std::function<Json(const std::string&)> createUserMessage;
  std::function<event_loop::Deferred<Json>(PoetryQueryRequest)>
    queryModelWithoutStreaming;
  std::function<std::string(const Json&)> extractTextContent;
  std::function<void(std::size_t)> addResponseLength;
  std::function<std::optional<Json>(const std::string&)> safeParseJSON;
  std::function<HookResponseParse(const Json&)> schemaSafeParse;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<std::string(std::exception_ptr)> errorMessage;
};
class PoetryHookHost {
public:
  PoetryHookHost(event_loop::Loop& loop,PoetryHookServices services);
  event_loop::Deferred<HookExecutionResult> execute(PoetryHookCall call);
  std::size_t activeCount()const;
private:
  struct Run;
  struct State;
  std::shared_ptr<State> state_;
};
} // namespace batbox::instructions::hooks::runtime
