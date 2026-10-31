#include "application_hooks/Provider.hpp"
#include "application/Async.hpp"
#include "messages/Context.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::application_hooks {
namespace {
std::string stringField(const Json& value,const char* key){
  const auto& found=value::property(value,key);
  return found.is_string()?found.get<std::string>():std::string{};
}
Json attachment(const std::string& type,const std::string& event,
    const std::string& name,const std::string& id,Json extra=Json::object()){
  Json data={{"type",type},{"hookName",name},{"toolUseID",id},
    {"hookEvent",event}};
  if(extra.is_object())for(auto it=extra.begin();it!=extra.end();++it)
    data[it.key()]=it.value();
  return Json{{"type","attachment"},{"attachment",data}};
}
Json hookDetail(const std::string& name,const std::string& id,
    const Json& input,const execution::Context& context){
  Json detail={{"tool_name",name},{"tool_use_id",id},{"tool_input",input}};
  const auto& bat=value::property(context.data,"agentId");
  if(bat.is_string())detail["agent_id"]=bat;
  const auto& type=value::property(context.data,"agentType");
  if(type.is_string())detail["agent_type"]=type;
  return detail;
}
}
void Provider::bindExecution(execution::Services& services,
    PermissionImports permission){
  auto self=shared_from_this();
  services.resolvePermissionDecision=[permission=std::move(permission)](
      const Json& hook,const gadgets::Gadget& gadget,const Json& input,
      const execution::Context& context,const Json& assistant,
      const std::string& id){
    return resolveHookPermissionDecision(hook,gadget,input,context,assistant,
      id,permission);
  };
  services.preGadgetUseHooks=[self](const gadgets::Gadget& gadget,
      const Json& input,const Json& gadgetUse,const execution::Context& context,
      const std::function<void(const Json&)>& emit){
    const auto id=stringField(gadgetUse,"id");
    const auto name="PreToolUse:"+gadget.name;
    try{
      self->streamSource("PreToolUse",
        hookDetail(gadget.name,id,input,context),gadget.name,context.stop,
        context,{},[&](const Json& row){
        if(value::truthy(value::property(row,"message")))
          emit({{"type","message"},{"message",{{"message",row.at("message")}}}});
        const auto& blocking=value::property(row,"blockingError");
        if(value::truthy(blocking)){
          const auto text=name+" hook error: "+stringField(blocking,"blockingError");
          emit({{"type","hookPermissionResult"},{"hookPermissionResult",
            {{"behavior","deny"},{"message",text},{"decisionReason",
              {{"type","hook"},{"hookName",name},{"reason",text}}}}}});
        }
        if(value::truthy(value::property(row,"preventContinuation"))){
          emit({{"type","preventContinuation"},
            {"shouldPreventContinuation",true}});
          if(value::truthy(value::property(row,"stopReason")))
            emit({{"type","stopReason"},
              {"stopReason",row.at("stopReason")}});
        }
        const auto behavior=stringField(row,"permissionBehavior");
        if(!behavior.empty()){
          Json reason={{"type","hook"},{"hookName",name}};
          if(row.contains("hookSource"))reason["hookSource"]=row.at("hookSource");
          if(row.contains("hookPermissionDecisionReason"))
            reason["reason"]=row.at("hookPermissionDecisionReason");
          Json decision={{"behavior",behavior},{"decisionReason",reason}};
          if((behavior=="allow"||behavior=="ask")&&row.contains("updatedInput"))
            decision["updatedInput"]=row.at("updatedInput");
          if(behavior!="allow"){
            const auto userReason=stringField(row,"hookPermissionDecisionReason");
            decision["message"]=userReason.empty()
              ?"Hook "+name+(behavior=="ask"?" asked to approve this tool":
                " denied this tool"):userReason;
          }
          emit({{"type","hookPermissionResult"},
            {"hookPermissionResult",std::move(decision)}});
        }else if(value::truthy(value::property(row,"updatedInput")))
          emit({{"type","hookUpdatedInput"},
            {"updatedInput",row.at("updatedInput")}});
        const auto& contexts=value::property(row,"additionalContexts");
        if(contexts.is_array()&&!contexts.empty())
          emit({{"type","additionalContext"},{"message",{{"message",
            attachment("hook_additional_context","PreToolUse",name,id,
              {{"content",contexts}})}}}});
        return true;});
    }catch(...){emit({{"type","stop"}});}
  };
  services.postGadgetUseHooks=[self](const gadgets::Gadget& gadget,
      const Json& input,const Json& output,const execution::Context& context,
      const std::function<void(const Json&)>& emit){
    try{
      auto detail=hookDetail(gadget.name,"",input,context);detail["tool_response"]=output;
      self->streamSource("PostToolUse",std::move(detail),gadget.name,
        context.stop,context,{},[&](const Json& row){
        const auto& message=value::property(row,"message");
        if(value::truthy(message)&&
            value::property(value::property(message,"attachment"),"type")!=
              "hook_blocking_error")emit({{"message",message}});
        if(value::truthy(value::property(row,"blockingError")))
          emit({{"message",self->inputs_.createAttachmentMessage(
            value::property(attachment("hook_blocking_error","PostToolUse",
              "PostToolUse:"+gadget.name,"",{{"blockingError",
                row.at("blockingError")}}),"attachment"))}});
        if(value::truthy(value::property(row,"preventContinuation")))
          emit({{"message",self->inputs_.createAttachmentMessage(
            value::property(attachment("hook_stopped_continuation","PostToolUse",
              "PostToolUse:"+gadget.name,"",{{"message",
                row.value("stopReason",std::string("Execution stopped by PostToolUse hook"))}}),
              "attachment"))}});
        if(row.contains("additionalContexts"))
          emit({{"message",self->inputs_.createAttachmentMessage(
            value::property(attachment("hook_additional_context","PostToolUse",
              "PostToolUse:"+gadget.name,"",{{"content",
                row.at("additionalContexts")}}),"attachment"))}});
        if(gadget.isMcp.value_or(false)&&row.contains("updatedMCPToolOutput"))
          emit(row);
        return true;});
    }catch(...){self->inputs_.log("PostToolUse hook execution failed");}
  };
  services.postGadgetUseFailureHooks=[self](const gadgets::Gadget& gadget,
      const Json& input,const std::string& error,bool isInterrupt,
      const execution::Context& context,
      const std::function<void(const Json&)>& emit){
    try{
      auto detail=hookDetail(gadget.name,"",input,context);
      detail["error"]=error;detail["is_interrupt"]=isInterrupt;
      self->streamSource("PostToolUseFailure",std::move(detail),gadget.name,
        context.stop,context,{},[&](const Json& row){
        const auto& message=value::property(row,"message");
        if(value::truthy(message)&&
            value::property(value::property(message,"attachment"),"type")!=
              "hook_blocking_error")emit({{"message",message}});
        if(value::truthy(value::property(row,"blockingError")))
          emit({{"message",self->inputs_.createAttachmentMessage(
            value::property(attachment("hook_blocking_error","PostToolUseFailure",
              "PostToolUseFailure:"+gadget.name,"",{{"blockingError",
                row.at("blockingError")}}),"attachment"))}});
        if(row.contains("additionalContexts"))
          emit({{"message",self->inputs_.createAttachmentMessage(
            value::property(attachment("hook_additional_context",
              "PostToolUseFailure","PostToolUseFailure:"+gadget.name,"",
              {{"content",row.at("additionalContexts")}}),"attachment"))}});
        return true;});
    }catch(...){self->inputs_.log("PostToolUseFailure hook execution failed");}
  };
}
void Provider::bindStop(query::StopHookServices& services){
  auto self=shared_from_this();const auto owner=owner_.dispatcher();
  services.executeStopHooks=[self,owner](const Json& args,
      const execution::Context& context,
      const query::HookResultSink& sink){
    const auto bat=stringField(args,"agentId");
    const auto event=bat.empty()?"Stop":"SubagentStop";
    const auto active=value::truthy(value::property(args,"stopHookActive"));
    const auto mode=stringField(args,"permissionMode");
    const auto batType=stringField(args,"agentType");
    const auto& messages=value::property(args,"messages");
    std::optional<std::string> lastText;
    if(messages.is_array()){
      const auto last=messages::getLastAssistantMessage(messages);
      if(!last.is_null())lastText=messages::getAssistantMessageText(last);
    }
    Json detail={{"stop_hook_active",active}};
    if(!mode.empty())detail["permission_mode"]=mode;
    if(!bat.empty()){
      detail["agent_id"]=bat;
      detail["agent_type"]=batType;
      detail["agent_transcript_path"]=
        application::onOwnerAndWait<std::string>(owner,[self,bat]{
          if(!self->inputs_.batTranscriptPath)
            throw std::logic_error("Subagent Stop transcript path import missing");
          return self->inputs_.batTranscriptPath(bat);
        });
    }
    if(lastText)detail["last_assistant_message"]=*lastText;
    self->streamSource(event,std::move(detail),{},context.stop,context,
      messages,sink);
  };
}
event_loop::Deferred<hooks::CompactHookOutput> Provider::preCompact(
    const std::string& trigger,
    const std::optional<std::string>& customInstructions,
    std::optional<std::stop_token> signal){
  return hooks::executePreCompactHooks(owner_,compact_,trigger,
    customInstructions,signal);
}
event_loop::Deferred<hooks::CompactHookOutput> Provider::postCompact(
    const std::string& trigger,const std::string& summary,
    std::optional<std::stop_token> signal){
  return hooks::executePostCompactHooks(owner_,compact_,trigger,summary,signal);
}
hooks::HookEventGenerator Provider::userPoetrySubmit(
    const std::string& poetry,const std::string& permissionMode,
    std::stop_token signal){
  auto self=shared_from_this();
  return hooks::HookEventGenerator(owner_,
    [poetry,permissionMode,signal]()->std::optional<hooks::ExecuteHooksInput>{
      hooks::ExecuteHooksInput call;
      call.batch.hookInput={{"hook_event_name","UserPromptSubmit"},
        {"prompt",poetry},{"permission_mode",permissionMode}};
      call.batch.signal=signal;
      return call;},
    [self](hooks::ExecuteHooksInput call){
      return self->makeSourceCursor("UserPromptSubmit",
        std::move(call.batch.hookInput),{},
        call.batch.signal.value_or(std::stop_token{}),{},{});
    });
}
} // namespace batbox::application_hooks
