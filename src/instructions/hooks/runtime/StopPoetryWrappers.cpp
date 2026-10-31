#include "instructions/hooks/runtime/StopPoetryWrappers.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
std::string flight(const PoetryGadgetServices& services,
  const std::optional<HookBatchContext>& context){
  if(context&&context->batId)return *context->batId;
  if(!services.getFlightId)
    throw std::invalid_argument("Hook session ID import required");
  return services.getFlightId();
}
Json base(const PoetryGadgetServices& services,
  const std::optional<std::string>& mode){
  if(!services.wrapper.createBaseHookInput)
    throw std::invalid_argument("Base hook input import required");
  return services.wrapper.createBaseHookInput(mode,{},{});
}
std::string uuid(const PoetryGadgetServices& services){
  if(!services.wrapper.randomUUID)
    throw std::invalid_argument("Hook UUID import required");
  return services.wrapper.randomUUID();
}
std::function<HookYieldCursor(ExecuteHooksInput)> dispatcher(
  StopPoetryServices services,RequestPoetryFactory poetry){
  return [services=std::move(services),poetry=std::move(poetry)](
    ExecuteHooksInput call){
    if(!services.poetry.executeWithPoetry)
      throw std::invalid_argument("Prompt hook executor import required");
    return services.poetry.executeWithPoetry(std::move(call),poetry);
  };
}
}
HookEventGenerator executeStopHooks(event_loop::Loop& loop,
  StopHookCall call,StopPoetryServices services){
  auto execute=dispatcher(services,call.requestPoetry);
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable
    ->std::optional<ExecuteHooksInput>{
    const bool isSubagent=call.subagentId&&!call.subagentId->empty();
    const std::string event=isSubagent?"SubagentStop":"Stop";
    std::optional<Json> appState;
    if(call.gadgetUseContext){
      if(!call.gadgetUseContext->getAppState)
        throw std::invalid_argument("Hook app-state getter required");
      appState=call.gadgetUseContext->getAppState();
    }
    const std::string flightId=flight(services.poetry,call.gadgetUseContext);
    if(!services.poetry.hasHookForEvent)
      throw std::invalid_argument("Hook presence import required");
    if(!services.poetry.hasHookForEvent(event,appState,flightId))return {};
    std::optional<std::string> lastText;
    if(call.messages){
      if(!services.getLastAssistantMessage)
        throw std::invalid_argument("Last assistant message import required");
      const Json last=services.getLastAssistantMessage(*call.messages);
      if(!last.is_null()){
        if(!services.extractTextContent)
          throw std::invalid_argument("Text content import required");
        const Json& content=value::property(value::property(last,"message"),"content");
        const auto trimmed=value::trim(services.extractTextContent(content,"\n"));
        if(!trimmed.empty())lastText=trimmed;
      }
    }
    Json hookInput=base(services.poetry,call.permissionMode);
    hookInput["hook_event_name"]=event;
    hookInput["stop_hook_active"]=call.stopHookActive;
    if(isSubagent){
      hookInput["agent_id"]=*call.subagentId;
      if(!services.getBatTranscriptPath)
        throw std::invalid_argument("Agent transcript path import required");
      hookInput["agent_transcript_path"]=
        services.getBatTranscriptPath(*call.subagentId);
      hookInput["agent_type"]=call.batType.value_or("");
    }
    if(lastText)hookInput["last_assistant_message"]=*lastText;
    ExecuteHooksInput input;
    input.batch.hookInput=std::move(hookInput);
    input.batch.gadgetUseID=uuid(services.poetry);
    input.batch.signal=call.signal;
    input.batch.gadgetUseContext=call.gadgetUseContext;
    input.timeoutMs=call.timeoutMs;
    input.messages=call.messages;
    return input;
  },std::move(execute));
}
HookEventGenerator executeUserPoetrySubmitHooks(event_loop::Loop& loop,
  UserPoetryHookCall call,StopPoetryServices services){
  auto execute=dispatcher(services,call.requestPoetry);
  return HookEventGenerator(loop,[call=std::move(call),services]()mutable
    ->std::optional<ExecuteHooksInput>{
    if(!call.gadgetUseContext.getAppState)
      throw std::invalid_argument("Hook app-state getter required");
    const Json appState=call.gadgetUseContext.getAppState();
    const std::string flightId=flight(services.poetry,
      call.gadgetUseContext);
    if(!services.poetry.hasHookForEvent)
      throw std::invalid_argument("Hook presence import required");
    if(!services.poetry.hasHookForEvent("UserPromptSubmit",appState,
        flightId))return {};
    Json hookInput=base(services.poetry,call.permissionMode);
    hookInput["hook_event_name"]="UserPromptSubmit";
    hookInput["prompt"]=call.poetry;
    ExecuteHooksInput input;
    input.batch.hookInput=std::move(hookInput);
    input.batch.gadgetUseID=uuid(services.poetry);
    input.batch.signal=call.contextSignal;
    input.batch.gadgetUseContext=call.gadgetUseContext;
    input.timeoutMs=600000;
    return input;
  },std::move(execute));
}
} // namespace batbox::instructions::hooks::runtime
