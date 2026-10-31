#include "instructions/hooks/runtime/HookBatchStart.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
std::string text(const Json& object,const char* key){
  const auto& value=value::property(object,key);
  return value.is_string()?value.get<std::string>():std::string{};
}
}
struct HookBatchStart::State:std::enable_shared_from_this<State>{
  enum class Stage { Initial,Matching,Internal,Progress,Closed };
  event_loop::Loop& loop;
  HookBatchStartInput input;
  HookBatchStartServices services;
  std::deque<event_loop::Deferred<HookBatchStartStep>> demands;
  std::vector<HookBatchMatch> matches;
  std::optional<Json> appState;
  std::string flightId,hookEvent,hookName;
  BoundHookPoetry boundPoetry;
  Stage stage=Stage::Initial;
  std::size_t internalIndex=0,progressIndex=0;
  double internalStart=0;
  bool internalWaiting=false;
  State(event_loop::Loop& owner,HookBatchStartInput value,
    HookBatchStartServices imports):loop(owner),input(std::move(value)),
    services(std::move(imports)){}
  void need(bool exists,const char* name){
    if(!exists)throw std::invalid_argument(name);
  }
  void complete(){
    stage=Stage::Closed;
    while(!demands.empty()){
      auto demand=demands.front();demands.pop_front();
      demand.resolve(HookBatchStartStep{});
    }
  }
  void fail(std::exception_ptr error){
    stage=Stage::Closed;
    if(!demands.empty()){
      auto demand=demands.front();demands.pop_front();demand.reject(error);
    }
    complete();
  }
  void start(){
    need(static_cast<bool>(services.shouldDisableAllHooksIncludingManaged),
      "Hook managed-disable import required");
    if(services.shouldDisableAllHooksIncludingManaged()){complete();return;}
    need(static_cast<bool>(services.isSimpleEnvironment),
      "Hook simple-environment import required");
    if(services.isSimpleEnvironment()){complete();return;}
    hookEvent=text(input.hookInput,"hook_event_name");
    if(hookEvent.empty())throw std::invalid_argument("Hook event required");
    hookName=input.matchQuery&&!input.matchQuery->empty()
      ?hookEvent+":"+*input.matchQuery:hookEvent;
    if(services.bindRequestPoetry)
      boundPoetry=services.bindRequestPoetry(hookName,input.gadgetInputSummary);
    need(static_cast<bool>(services.shouldSkipHookDueToTrust),
      "Hook workspace-trust import required");
    if(services.shouldSkipHookDueToTrust()){
      need(static_cast<bool>(services.logForDebugging),
        "Hook local debug import required");
      services.logForDebugging("Skipping "+hookName+
        " hook execution - workspace trust not accepted");
      complete();return;
    }
    if(input.gadgetUseContext){
      need(static_cast<bool>(input.gadgetUseContext->getAppState),
        "Hook app-state getter required");
      appState=input.gadgetUseContext->getAppState();
    }
    if(input.gadgetUseContext&&input.gadgetUseContext->batId)
      flightId=*input.gadgetUseContext->batId;
    else{
      need(static_cast<bool>(services.getFlightId),"Hook session ID import required");
      flightId=services.getFlightId();
    }
    need(static_cast<bool>(services.getMatchingHooks),
      "Hook matcher import required");
    stage=Stage::Matching;
    const auto gadgets=input.gadgetUseContext?input.gadgetUseContext->gadgets:
      std::optional<Json>{};
    const auto weak=weak_from_this();
    services.getMatchingHooks(appState,flightId,hookEvent,input.hookInput,
      gadgets).then([weak](const std::vector<HookBatchMatch>* result,
        const std::exception_ptr& error){
      if(auto state=weak.lock()){
        if(error){state->fail(error);return;}
        try{
          state->matches=*result;
          if(state->matches.empty()){state->complete();return;}
          if(state->input.signal&&state->input.signal->stop_requested()){
            state->complete();return;
          }
          state->need(static_cast<bool>(state->services.isInternalHook),
            "Internal hook classifier required");
          bool hasUser=false;
          for(const auto& match:state->matches)
            if(!state->services.isInternalHook(match))hasUser=true;
          if(hasUser){state->stage=Stage::Progress;state->pump();return;}
          state->need(static_cast<bool>(state->services.nowMillis),
            "Hook clock import required");
          state->internalStart=state->services.nowMillis();
          state->stage=Stage::Internal;state->runInternal();
        }catch(...){state->fail(std::current_exception());}
      }
    });
  }
  void runInternal(){
    if(stage!=Stage::Internal||internalWaiting)return;
    try{
      while(internalIndex<matches.size()){
        const auto index=internalIndex++;
        const auto& match=matches[index];
        if(!match.hook)throw std::invalid_argument("Hook identity required");
        if(text(match.hook->fields,"type")!="callback")continue;
        if(!match.callback||!match.callback->callback)
          throw std::invalid_argument("Internal hook callback required");
        std::optional<HookCallbackContext> context;
        if(input.gadgetUseContext)context=HookCallbackContext{
          input.gadgetUseContext->getAppState,
          input.gadgetUseContext->updateAttributionState};
        auto reply=match.callback->callback(input.hookInput,input.gadgetUseID,
          input.signal.value_or(std::stop_token{}),index,context);
        event_loop::Deferred<Json> pending(loop);
        if(std::holds_alternative<Json>(reply))
          pending.resolve(std::get<Json>(std::move(reply)));
        else pending=std::get<event_loop::Deferred<Json>>(std::move(reply));
        internalWaiting=true;
        const auto weak=weak_from_this();
        pending.then([weak](const Json*,const std::exception_ptr& error){
          if(auto state=weak.lock()){
            state->internalWaiting=false;
            if(error){state->fail(error);return;}
            state->runInternal();
          }
        });
        return;
      }
      need(static_cast<bool>(services.nowMillis),"Hook clock import required");
      need(static_cast<bool>(services.addToTurnHookDuration),
        "Hook duration import required");
      services.addToTurnHookDuration(services.nowMillis()-internalStart);
      complete();
    }catch(...){fail(std::current_exception());}
  }
  void progress(){
    if(progressIndex>=matches.size()){
      need(static_cast<bool>(services.nowMillis),"Hook clock import required");
      HookBatchStartStep step;
      step.kind=HookBatchStartStep::Kind::Ready;
      step.matches=matches;step.appState=appState;
      step.flightId=flightId;step.hookEvent=hookEvent;
      step.hookName=hookName;step.batchStartTime=services.nowMillis();
      step.boundRequestPoetry=boundPoetry;
      stage=Stage::Closed;
      auto demand=demands.front();demands.pop_front();
      demand.resolve(std::move(step));
      complete();return;
    }
    need(static_cast<bool>(services.getHookDisplayText),
      "Hook display import required");
    need(static_cast<bool>(services.nowIso),"Hook timestamp import required");
    need(static_cast<bool>(services.randomUUID),"Hook UUID import required");
    const auto& match=matches[progressIndex++];
    if(!match.hook)throw std::invalid_argument("Hook identity required");
    const auto& hook=match.hook->fields;
    Json data={{"type","hook_progress"},{"hookEvent",hookEvent},
      {"hookName",hookName},
      {"command",services.getHookDisplayText(match)}};
    HookBatchStartStep step;step.kind=HookBatchStartStep::Kind::Progress;
    if(text(hook,"type")=="prompt"){
      if(hook.contains("prompt"))data["promptText"]=hook.at("prompt");
      else step.ownUndefinedKeys.push_back("message.data.promptText");
    }
    if(hook.contains("statusMessage")&&!hook.at("statusMessage").is_null())
      data["statusMessage"]=hook.at("statusMessage");
    const auto timestamp=services.nowIso();
    const auto uuid=services.randomUUID();
    step.progress={{"message",Json{{"type","progress"},{"data",data},
      {"parentToolUseID",input.gadgetUseID},{"toolUseID",input.gadgetUseID},
      {"timestamp",timestamp},{"uuid",uuid}}}};
    auto demand=demands.front();demands.pop_front();
    demand.resolve(std::move(step));
    if(!demands.empty()){
      const auto weak=weak_from_this();
      loop.queueMicrotask([weak]{if(auto state=weak.lock())state->pump();});
    }
  }
  void pump(){
    if(stage==Stage::Closed||demands.empty())return;
    try{
      if(stage==Stage::Initial)start();
      else if(stage==Stage::Progress)progress();
    }catch(...){fail(std::current_exception());}
  }
};
HookBatchStart::HookBatchStart(event_loop::Loop& loop,
  HookBatchStartInput input,HookBatchStartServices services):
  state_(std::make_shared<State>(loop,std::move(input),std::move(services))){}
event_loop::Deferred<HookBatchStartStep> HookBatchStart::next(){
  auto& state=*state_;
  if(!state.loop.isOwnerThread())
    throw std::logic_error("Hook batch owner required");
  event_loop::Deferred<HookBatchStartStep> output(state.loop);
  if(state.stage==State::Stage::Closed)output.resolve(HookBatchStartStep{});
  else{state.demands.push_back(output);state.pump();}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
