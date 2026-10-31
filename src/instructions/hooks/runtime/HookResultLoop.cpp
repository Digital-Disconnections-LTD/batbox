#include "instructions/hooks/runtime/HookResultLoop.hpp"
#include <deque>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
std::string stringField(const Json& object,const char* key){
  const auto& value=field(object,key);
  return value.is_string()?value.get<std::string>():std::string{};
}
std::string joinedKeys(const Json& object){
  const auto keys=value::objectKeys(object);
  std::string value;
  for(const auto& key:keys){if(!value.empty())value+=", ";value+=key;}
  return value;
}
}
struct HookResultLoop::State:std::enable_shared_from_this<State>{
  event_loop::Loop& loop;
  HookResultLoopInput input;
  HookResultLoopServices services;
  std::deque<event_loop::Deferred<HookYield>> demands;
  std::optional<HookExecutionResult> current;
  std::optional<std::string> permissionBehavior;
  int phase=0;
  bool waitingResult=false,closed=false;
  State(event_loop::Loop& owner,HookResultLoopInput value,
    HookResultLoopServices imports):loop(owner),input(std::move(value)),
    services(std::move(imports)){}
  void require(bool supplied,const char* name){
    if(!supplied)throw std::invalid_argument(name);
  }
  std::string display(){
    require(static_cast<bool>(services.getHookDisplayText),
      "Hook display import required");
    if(!current->hook)throw std::invalid_argument("Hook result identity required");
    return services.getHookDisplayText(*current->hook);
  }
  void debug(const std::string& message){
    require(static_cast<bool>(services.logForDebugging),
      "Hook debug import required");
    services.logForDebugging(message);
  }
  void yield(Json value,std::vector<std::string> undefined={}){
    auto demand=demands.front();demands.pop_front();
    demand.resolve(HookYield{false,std::move(value),std::move(undefined)});
    if(!demands.empty()){
      const auto weak=weak_from_this();
      loop.queueMicrotask([weak]{if(auto state=weak.lock())state->pump();});
    }
  }
  void done(){
    closed=true;
    while(!demands.empty()){
      auto demand=demands.front();demands.pop_front();
      demand.resolve(HookYield{true,Json(nullptr),{}});
    }
  }
  void fail(std::exception_ptr error){
    closed=true;
    if(!demands.empty()){
      auto demand=demands.front();demands.pop_front();
      demand.reject(error);
    }
    done();
  }
  std::string label(const std::string& action){
    return "Hook "+input.hookEvent+" ("+display()+") "+action;
  }
  bool advance(){
    const auto& fields=current->fields;
    while(phase<=14){
      const auto step=phase++;
      if(step==0&&value::truthy(field(fields,"preventContinuation"))){
        debug(label("requested preventContinuation"));
        Json value={{"preventContinuation",true}};
        std::vector<std::string> undefined;
        if(fields.contains("stopReason"))value["stopReason"]=fields.at("stopReason");
        else undefined.push_back("stopReason");
        yield(std::move(value),std::move(undefined));return true;
      }
      if(step==1&&value::truthy(field(fields,"blockingError"))){
        yield(Json{{"blockingError",fields.at("blockingError")}});return true;
      }
      if(step==2&&value::truthy(field(fields,"message"))){
        yield(Json{{"message",fields.at("message")}});return true;
      }
      if(step==3&&value::truthy(field(fields,"systemMessage"))){
        require(static_cast<bool>(services.createAttachmentMessage),
          "Hook attachment import required");
        const Json attachment={{"type","hook_system_message"},
          {"content",fields.at("systemMessage")},
          {"hookName",input.hookName},{"toolUseID",input.gadgetUseID},
          {"hookEvent",input.hookEvent}};
        yield(Json{{"message",services.createAttachmentMessage(attachment)}});
        return true;
      }
      if(step==4&&value::truthy(field(fields,"additionalContext"))){
        const auto context=stringField(fields,"additionalContext");
        debug(label("provided additionalContext ("+
          std::to_string(value::stringLength(context))+" chars)"));
        yield(Json{{"additionalContexts",Json::array({context})}});return true;
      }
      if(step==5&&value::truthy(field(fields,"initialUserMessage"))){
        const auto message=stringField(fields,"initialUserMessage");
        debug(label("provided initialUserMessage ("+
          std::to_string(value::stringLength(message))+" chars)"));
        yield(Json{{"initialUserMessage",message}});return true;
      }
      if(step==6&&fields.contains("watchPaths")&&
        fields.at("watchPaths").is_array()&&!fields.at("watchPaths").empty()){
        debug(label("provided "+std::to_string(fields.at("watchPaths").size())+
          " watchPaths"));
        yield(Json{{"watchPaths",fields.at("watchPaths")}});return true;
      }
      if(step==7&&value::truthy(field(fields,"updatedMCPToolOutput"))){
        debug(label("replaced MCP tool output"));
        yield(Json{{"updatedMCPToolOutput",fields.at("updatedMCPToolOutput")}});
        return true;
      }
      if(step==8){
        const auto behavior=stringField(fields,"permissionBehavior");
        if(!behavior.empty()){
          const auto reason=stringField(fields,"hookPermissionDecisionReason");
          debug(label("returned permissionDecision: "+behavior+
            (reason.empty()?std::string{}:" (reason: "+reason+")")));
          if(behavior=="deny")permissionBehavior="deny";
          else if(behavior=="ask"){
            if(permissionBehavior!="deny")permissionBehavior="ask";
          }else if(behavior=="allow"){
            if(!permissionBehavior)permissionBehavior="allow";
          }
        }
        if(permissionBehavior){
          const auto& updated=field(fields,"updatedInput");
          const bool hasUpdated=value::truthy(updated)&&
            (behavior=="allow"||behavior=="ask");
          if(hasUpdated)debug(label("modified tool input keys: ["+
            joinedKeys(updated)+"]"));
          Json value={{"permissionBehavior",*permissionBehavior}};
          std::vector<std::string> undefined;
          if(fields.contains("hookPermissionDecisionReason"))
            value["hookPermissionDecisionReason"]=
              fields.at("hookPermissionDecisionReason");
          else undefined.push_back("hookPermissionDecisionReason");
          std::optional<std::string> source;
          for(const auto& match:input.matchingHooks)
            if(match.hook==current->hook){source=match.hookSource;break;}
          if(source)value["hookSource"]=*source;
          else undefined.push_back("hookSource");
          if(hasUpdated)value["updatedInput"]=updated;
          else undefined.push_back("updatedInput");
          yield(std::move(value),std::move(undefined));return true;
        }
      }
      if(step==9&&value::truthy(field(fields,"updatedInput"))&&
        !fields.contains("permissionBehavior")){
        const auto& updated=fields.at("updatedInput");
        debug(label("modified tool input keys: ["+joinedKeys(updated)+"]"));
        yield(Json{{"updatedInput",updated}});return true;
      }
      if(step==10&&value::truthy(field(fields,"permissionRequestResult"))){
        yield(Json{{"permissionRequestResult",
          fields.at("permissionRequestResult")}});return true;
      }
      if(step==11&&value::truthy(field(fields,"retry"))){
        yield(Json{{"retry",fields.at("retry")}});return true;
      }
      if(step==12&&value::truthy(field(fields,"elicitationResponse"))){
        yield(Json{{"elicitationResponse",fields.at("elicitationResponse")}});
        return true;
      }
      if(step==13&&value::truthy(field(fields,"elicitationResultResponse"))){
        yield(Json{{"elicitationResultResponse",
          fields.at("elicitationResultResponse")}});return true;
      }
      if(step==14){
        if(input.appState&&value::truthy(*input.appState)&&
          current->hook&&stringField(current->hook->fields,"type")!="callback"){
          require(static_cast<bool>(services.getFlightId),
            "Hook session ID import required");
          require(static_cast<bool>(services.getFlightHookCallback),
            "Session hook callback lookup import required");
          const auto flightId=services.getFlightId();
          const auto entry=services.getFlightHookCallback(*input.appState,
            flightId,input.hookEvent,input.matchQuery.value_or(""),current->hook);
          if(entry&&current->outcome=="success"){
            try{(*entry)(current->hook,*current);}
            catch(...){
              require(static_cast<bool>(services.logError),
                "Hook local error import required");
              services.logError("Session hook success callback failed",
                std::current_exception());
            }
          }
        }
      }
    }
    current.reset();phase=0;return false;
  }
  void pump(){
    if(closed||waitingResult||demands.empty())return;
    try{
      while(!demands.empty()){
        if(current){if(advance())return;continue;}
        require(static_cast<bool>(services.nextResult),
          "Hook result iterator import required");
        waitingResult=true;
        const auto weak=weak_from_this();
        services.nextResult().then([weak](
          const GeneratorStep<HookExecutionResult>* step,
          const std::exception_ptr& failure){
          if(auto state=weak.lock()){
            state->waitingResult=false;
            if(failure){state->fail(failure);return;}
            try{
              if(step->done){
                state->require(static_cast<bool>(state->services.nowMillis),
                  "Hook clock import required");
                state->require(static_cast<bool>(state->services.addToTurnHookDuration),
                  "Hook duration import required");
                const auto duration=state->services.nowMillis()-
                  state->input.batchStartTime;
                state->services.addToTurnHookDuration(duration);
                state->done();return;
              }
              state->current=*step->value;state->phase=0;state->pump();
            }catch(...){state->fail(std::current_exception());}
          }
        });
        return;
      }
    }catch(...){fail(std::current_exception());}
  }
};
HookResultLoop::HookResultLoop(event_loop::Loop& loop,HookResultLoopInput input,
  HookResultLoopServices services):state_(std::make_shared<State>(loop,
    std::move(input),std::move(services))){}
event_loop::Deferred<HookYield> HookResultLoop::next(){
  auto& state=*state_;
  if(!state.loop.isOwnerThread())
    throw std::logic_error("Hook result loop owner required");
  event_loop::Deferred<HookYield> output(state.loop);
  if(state.closed)output.resolve(HookYield{true,Json(nullptr),{}});
  else{state.demands.push_back(output);state.pump();}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
