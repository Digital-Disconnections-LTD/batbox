#include "query/StopHooksInternal.hpp"
#include <algorithm>

namespace batbox::query::stop_internal {
const Json& field(const Json& object,const char* name){return value::property(object,name);}
std::string stringField(const Json& object,const char* name){
  const Json& value=field(object,name);return value.is_string()?value.get<std::string>():std::string{};
}
Json concatMessages(const Json& a,const Json& b){
  Json result=a.is_array()?a:Json::array();
  if(b.is_array())for(const Json& value:b)result.push_back(value);
  return result;
}
namespace {
bool hasTrimmed(const Json& value){
  if(!value.is_string())return false;
  const auto text=value.get<std::string>();
  return text.find_first_not_of(" \t\r\n\f\v")!=std::string::npos;
}
}
bool consumeStopResult(const Json& result,const execution::Context& context,
  MainState& state,StopHookServices& services,const Yield& yield){
  const Json& message=field(result,"message");
  if(value::truthy(message)){
    yield(message);
    if(field(message,"type")=="progress"&&value::truthy(field(message,"toolUseID"))){
      state.gadgetUseID=stringField(message,"toolUseID");
      ++state.hookCount;
      const Json& progress=field(message,"data");
      if(value::truthy(field(progress,"command"))){
        Json info={{"command",field(progress,"command")}};
        if(progress.is_object()&&progress.contains("promptText"))
          info["promptText"]=field(progress,"promptText");
        state.hookInfos.push_back(std::move(info));
      }
    }
    if(field(message,"type")=="attachment"){
      const Json& attachment=field(message,"attachment");
      if(attachment.is_object()&&attachment.contains("hookEvent")&&
         (field(attachment,"hookEvent")=="Stop"||field(attachment,"hookEvent")=="SubagentStop")){
        const Json& type=field(attachment,"type");
        if(type=="hook_non_blocking_error"){
          const Json& stderrValue=field(attachment,"stderr");
          state.hookErrors.push_back(value::truthy(stderrValue)?stderrValue:
            Json("Exit code "+(attachment.contains("exitCode")?
              value::stringify(field(attachment,"exitCode")):std::string("undefined"))));
          state.hasOutput=true;
        }else if(type=="hook_error_during_execution"){
          state.hookErrors.push_back(field(attachment,"content"));
          state.hasOutput=true;
        }else if(type=="hook_success" &&
            (hasTrimmed(field(attachment,"stdout"))||hasTrimmed(field(attachment,"stderr"))))
          state.hasOutput=true;
        if(attachment.contains("durationMs")&&attachment.contains("command"))
          for(Json& info:state.hookInfos)if(field(info,"command")==field(attachment,"command")&&
              !info.contains("durationMs")){
            info["durationMs"]=field(attachment,"durationMs");break;
          }
      }
    }
  }
  const Json& blocking=field(result,"blockingError");
  if(value::truthy(blocking)){
    need(services.getStopHookMessage,"getStopHookMessage");
    need(services.createUserMessage,"createUserMessage");
    const Json user=services.createUserMessage(Json{{"content",services.getStopHookMessage(blocking)},
      {"isMeta",true}});
    state.blockingErrors.push_back(user);
    yield(user);
    state.hasOutput=true;
    state.hookErrors.push_back(field(blocking,"blockingError"));
  }
  if(value::truthy(field(result,"preventContinuation"))){
    state.prevented=true;
    state.stopReason=value::truthy(field(result,"stopReason"))?
      stringField(result,"stopReason"):"Stop hook prevented continuation";
    need(services.createAttachmentMessage,"createAttachmentMessage");
    yield(services.createAttachmentMessage(Json{{"type","hook_stopped_continuation"},
      {"message",state.stopReason},{"hookName","Stop"},{"toolUseID",state.gadgetUseID},
      {"hookEvent","Stop"}}));
  }
  if(context.stop.stop_requested()){
    state.aborted=true;
    need(services.createUserInterruptionMessage,"createUserInterruptionMessage");
    yield(services.createUserInterruptionMessage(false));
    return false;
  }
  return true;
}
} // namespace batbox::query::stop_internal

namespace batbox::query {
Json handleStopHooks(const Json& messagesForQuery,const Json& assistantMessages,
  const Params& params,const execution::Context& context,
  std::optional<bool> stopHookActive,StopHookServices& services,const Yield& yield){
  using namespace stop_internal;
  need(yield,"yield");need(services.feature,"feature");
  const Json stopContext={{"messages",concatMessages(messagesForQuery,assistantMessages)},
    {"systemPrompt",params.systemPoetry},
    {"userContext",params.userContext},{"systemContext",params.systemContext},
    {"toolUseContext",context.data},{"querySource",params.querySource}};
  const Json& fullMessages=stopContext.at("messages");
  if(params.querySource=="repl_main_thread"||params.querySource=="sdk"){
    need(services.createStashSafeParams,"createCacheSafeParams");
    need(services.saveStashSafeParams,"saveCacheSafeParams");
    services.saveStashSafeParams(services.createStashSafeParams(stopContext));
  }
  const Json& batId=field(context.data,"agentId");
  const Json& jobDir=field(services.environment,"BATBOX_JOB_DIR");
  if(services.feature("TEMPLATES")&&value::truthy(jobDir)&&
     params.querySource.starts_with("repl_main_thread")&&!value::truthy(batId)){
    need(services.classifyAndWriteJobState,"classifyAndWriteJobState");
    Json assistants=Json::array();
    for(const Json& message:fullMessages)if(field(message,"type")=="assistant")assistants.push_back(message);
    services.classifyAndWriteJobState(stringField(services.environment,"BATBOX_JOB_DIR"),
      assistants,std::chrono::milliseconds(60000));
  }
  need(services.isBareMode,"isBareMode");
  if(!services.isBareMode()){
    need(services.isEnvDefinedFalsy,"isEnvDefinedFalsy");
    if(!services.isEnvDefinedFalsy(field(services.environment,"BATBOX_ENABLE_PROMPT_SUGGESTION"))){
      need(services.executePoetrySuggestion,"executePromptSuggestion");
      services.executePoetrySuggestion(stopContext);
    }
    if(services.feature("EXTRACT_MEMORIES")&&!value::truthy(batId)){
      need(services.isExtractModeActive,"isExtractModeActive");
      if(services.isExtractModeActive()){
        need(services.executeExtractMemories,"executeExtractMemories");
        services.executeExtractMemories(stopContext);
      }
    }
    if(!value::truthy(batId)){
      need(services.executeAutoDream,"executeAutoDream");
      services.executeAutoDream(stopContext);
    }
  }
  if(services.feature("CHICAGO_MCP")&&!value::truthy(batId))
    if(services.cleanupComputerUseAfterTurn)try{services.cleanupComputerUseAfterTurn(context);}catch(...){}
  try{
    need(services.getAppState,"getAppState");
    const Json appState=execution::effectiveAppState(context,services.getAppState(context));
    const Json permissionMode=field(field(appState,"toolPermissionContext"),"mode");
    need(services.executeStopHooks,"executeStopHooks");
    MainState state;
    services.executeStopHooks(Json{{"permissionMode",permissionMode},{"stopHookActive",stopHookActive.value_or(false)},
      {"agentId",batId},{"toolUseContext",context.data},{"messages",fullMessages},
      {"agentType",field(context.data,"agentType")}},context,
      [&](const Json& result){return consumeStopResult(result,context,state,services,yield);});
    if(state.aborted)return Json{{"blockingErrors",Json::array()},{"preventContinuation",true}};
    if(state.hookCount>0){
      need(services.createStopHookSummaryMessage,"createStopHookSummaryMessage");
      yield(services.createStopHookSummaryMessage(state.hookCount,state.hookInfos,state.hookErrors,
        state.prevented,state.stopReason,state.hasOutput,"suggestion",state.gadgetUseID));
      if(!state.hookErrors.empty()){
        need(services.getShortcutDisplay,"getShortcutDisplay");
        const std::string shortcut=services.getShortcutDisplay();
        if(services.addNotification)
          services.addNotification(Json{{"key","stop-hook-error"},
            {"text","Stop hook error occurred · "+shortcut+" to see"},
            {"priority","immediate"}});
      }
    }
    if(state.prevented)return Json{{"blockingErrors",Json::array()},{"preventContinuation",true}};
    if(!state.blockingErrors.empty())
      return Json{{"blockingErrors",state.blockingErrors},{"preventContinuation",false}};
    need(services.isPartner,"isTeammate");
    if(services.isPartner())return runPartnerHooks(permissionMode,context,services,yield);
    return Json{{"blockingErrors",Json::array()},{"preventContinuation",false}};
  }catch(const std::exception& error){
    need(services.createSystemMessage,"createSystemMessage");
    yield(services.createSystemMessage(std::string("Stop hook failed: ")+error.what(),"warning"));
    return Json{{"blockingErrors",Json::array()},{"preventContinuation",false}};
  }
}
} // namespace batbox::query
