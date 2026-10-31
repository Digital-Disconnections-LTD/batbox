#include "instructions/hooks/runtime/BatHookExecutor.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
const Json& field(const Json& object,const char* key){
  return value::property(object,key);
}
std::string stringField(const Json& object,const char* key){
  const auto& value=field(object,key);
  return value.is_string()?value.get<std::string>():std::string{};
}
}
struct BatHookHost::State {
  event_loop::Loop& loop;
  BatHookServices services;
  std::map<std::size_t,std::shared_ptr<Run>> active;
  std::size_t nextId=1;
  State(event_loop::Loop& owner,BatHookServices imports):loop(owner),
    services(std::move(imports)){}
};
struct BatHookHost::Run:std::enable_shared_from_this<Run>{
  std::weak_ptr<State> host;
  event_loop::Loop& loop;
  BatHookServices services;
  BatHookCall call;
  event_loop::Deferred<HookExecutionResult> output;
  std::shared_ptr<CombinedAbort> parentTimeout;
  std::unique_ptr<std::stop_callback<std::function<void()>>> parentListener;
  std::stop_source batAbort;
  BatMessageStream stream;
  std::optional<Json> structuredOutputResult;
  std::string effectiveGadgetUseID,transcriptPath,hookBatId;
  std::size_t id;
  int turnCount=0;
  bool hitMaxTurns=false,finished=false;
  Run(const std::shared_ptr<State>& owner,BatHookCall input,
    std::size_t key):host(owner),loop(owner->loop),services(owner->services),
    call(std::move(input)),output(loop),id(key){}
  void require(bool supplied,const char* name){
    if(!supplied)throw std::invalid_argument(name);
  }
  void debug(const std::string& message){
    require(static_cast<bool>(services.logForDebugging),
      "Agent hook local debug import required");
    services.logForDebugging(message);
  }
  HookExecutionResult result(const std::string& outcome,Json fields={}){
    fields["outcome"]=outcome;
    return {std::move(fields),outcome,call.hook};
  }
  Json attachment(Json payload){
    require(static_cast<bool>(services.createAttachmentMessage),
      "Agent hook attachment factory required");
    return services.createAttachmentMessage(payload);
  }
  Json attachmentBase(const char* type){
    return {{"type",type},{"hookName",call.hookName},
      {"toolUseID",effectiveGadgetUseID},{"hookEvent",call.hookEvent}};
  }
  void clean(){
    parentListener.reset();
    if(parentTimeout)parentTimeout->cleanup();
  }
  void finish(HookExecutionResult value){
    if(finished)return;
    finished=true;
    clean();output.resolve(std::move(value));
    if(auto state=host.lock())state->active.erase(id);
  }
  void fail(std::exception_ptr error){
    if(finished)return;
    finished=true;
    clean();output.reject(error);
    if(auto state=host.lock())state->active.erase(id);
  }
  void outerError(std::exception_ptr failure){
    try{
      require(static_cast<bool>(services.errorMessage),
        "Agent hook error formatter required");
      const auto message=services.errorMessage(failure);
      debug("Hooks: Agent hook error: "+message);
      auto payload=attachmentBase("hook_non_blocking_error");
      payload["stderr"]="Error executing agent hook: "+message;
      payload["stdout"]="";payload["exitCode"]=1;
      finish(result("non_blocking_error",{{"message",attachment(payload)}}));
    }catch(...){fail(std::current_exception());}
  }
  void innerError(std::exception_ptr failure){
    try{
      clean();
      if(batAbort.stop_requested()){
        finish(result("cancelled"));return;
      }
      outerError(failure);
    }catch(...){outerError(std::current_exception());}
  }
  Json batAppState(){
    require(static_cast<bool>(call.context.getAppState),
      "Agent hook app-state getter required");
    auto appState=call.context.getAppState();
    auto permission=appState.at("toolPermissionContext");
    auto allow=permission.at("alwaysAllowRules");
    Json flight=Json::array();
    if(allow.contains("session")&&!allow.at("session").is_null())
      flight=allow.at("session");
    flight.push_back("Read(/"+transcriptPath+")");
    allow["session"]=std::move(flight);
    permission["mode"]="dontAsk";
    permission["alwaysAllowRules"]=std::move(allow);
    appState["toolPermissionContext"]=std::move(permission);
    return appState;
  }
  void afterQuery(){
    try{
      clean();
      require(static_cast<bool>(services.clearFlightHooks),
        "Agent session hook cleanup import required");
      services.clearFlightHooks(hookBatId);
      if(!structuredOutputResult){
        if(hitMaxTurns)
          debug("Hooks: Agent hook did not complete within 50 turns");
        else debug("Hooks: Agent hook did not return structured output");
        finish(result("cancelled"));return;
      }
      if(!value::truthy(field(*structuredOutputResult,"ok"))){
        const auto reason=structuredOutputResult->contains("reason")?
          stringField(*structuredOutputResult,"reason"):
          std::string("undefined");
        debug("Hooks: Agent hook condition was not met: "+reason);
        finish(result("blocking",{{"blockingError",{{"blockingError",
          "Agent hook condition was not met: "+reason},
          {"command",stringField(call.hook->fields,"prompt")}}}}));
        return;
      }
      debug("Hooks: Agent hook condition was met");
      auto payload=attachmentBase("hook_success");
      payload["content"]="";
      finish(result("success",{{"message",attachment(payload)}}));
    }catch(...){innerError(std::current_exception());}
  }
  void closeStream(){
    try{
      require(static_cast<bool>(stream.close),
        "Agent query iterator return import required");
      const auto weak=weak_from_this();
      stream.close().then([weak](const GeneratorStep<Json>*,
        const std::exception_ptr& failure){
        if(auto run=weak.lock()){
          if(failure)run->innerError(failure);
          else run->afterQuery();
        }
      });
    }catch(...){innerError(std::current_exception());}
  }
  void nextMessage(){
    try{
      require(static_cast<bool>(stream.next),
        "Agent query iterator next import required");
      const auto weak=weak_from_this();
      stream.next().then([weak](const GeneratorStep<Json>* step,
        const std::exception_ptr& failure){
        if(auto run=weak.lock()){
          if(failure){run->innerError(failure);return;}
          if(step->done){run->afterQuery();return;}
          run->onMessage(*step->value);
        }
      });
    }catch(...){innerError(std::current_exception());}
  }
  void onMessage(const Json& message){
    try{
      require(static_cast<bool>(services.handleMessageFromStream),
        "Agent stream-message handler required");
      require(static_cast<bool>(services.addResponseLength),
        "Agent response-length import required");
      services.handleMessageFromStream(message,services.addResponseLength,
        services.setStreamMode);
      const auto type=stringField(message,"type");
      if(type=="stream_event"||type=="stream_request_start"){
        nextMessage();return;
      }
      if(type=="assistant"){
        ++turnCount;
        if(turnCount>=50){
          hitMaxTurns=true;
          debug("Hooks: Agent turn "+std::to_string(turnCount)+
            " hit max turns, aborting");
          batAbort.request_stop();closeStream();return;
        }
      }
      if(type=="attachment"&&
        stringField(message.at("attachment"),"type")=="structured_output"){
        require(static_cast<bool>(services.schemaSafeParse),
          "Agent hook response schema required");
        const auto parsed=services.schemaSafeParse(
          message.at("attachment").at("data"));
        if(parsed.success){
          structuredOutputResult=parsed.data;
          require(static_cast<bool>(services.jsonStringify),
            "Agent JSON stringify import required");
          debug("Hooks: Got structured output: "+
            services.jsonStringify(*structuredOutputResult));
          batAbort.request_stop();closeStream();return;
        }
      }
      nextMessage();
    }catch(...){innerError(std::current_exception());}
  }
  void start(){
    try{
      if(!call.hook)throw std::invalid_argument("Agent hook required");
      if(call.gadgetUseID.empty()){
        require(static_cast<bool>(services.randomUUID),
          "Agent hook UUID import required");
        effectiveGadgetUseID="hook-"+services.randomUUID();
      }else effectiveGadgetUseID=call.gadgetUseID;
      if(call.context.batId){
        require(static_cast<bool>(services.getBatTranscriptPath),
          "Agent transcript path import required");
        transcriptPath=services.getBatTranscriptPath(*call.context.batId);
      }else{
        require(static_cast<bool>(services.getTranscriptPath),
          "Transcript path import required");
        transcriptPath=services.getTranscriptPath();
      }
    }catch(...){fail(std::current_exception());return;}
    try{
      require(static_cast<bool>(services.addArgumentsToPoetry),
        "Agent argument substitution import required");
      const auto poetry=services.addArgumentsToPoetry(
        stringField(call.hook->fields,"prompt"),call.jsonInput);
      debug("Hooks: Processing agent hook with prompt: "+poetry);
      require(static_cast<bool>(services.createUserMessage),
        "Agent user-message factory required");
      const auto user=services.createUserMessage(poetry);
      const Json batMessages=Json::array({user});
      debug("Hooks: Starting agent query with 1 messages");
      const auto& timeout=field(call.hook->fields,"timeout");
      parentTimeout=CombinedAbort::create(loop,call.signal,std::nullopt,
        value::truthy(timeout)?timeout.get<double>()*1000:60000);
      if(!parentTimeout->aborted()){
        parentListener=
          std::make_unique<std::stop_callback<std::function<void()>>>(
            parentTimeout->signal(),[this]{batAbort.request_stop();});
      }
      require(static_cast<bool>(services.createStructuredOutputGadget),
        "Structured-output tool factory required");
      const auto structured=services.createStructuredOutputGadget();
      if(!call.context.gadgets||!call.context.gadgets->is_array())
        throw std::invalid_argument("Agent context tools required");
      require(static_cast<bool>(services.gadgetMatchesName),
        "Agent tool-name matcher required");
      if(services.structuredOutputGadgetName.empty())
        throw std::invalid_argument("Structured-output protocol name required");
      Json filtered=Json::array();
      for(const auto& gadget:*call.context.gadgets)
        if(!services.gadgetMatchesName(gadget,
          services.structuredOutputGadgetName))filtered.push_back(gadget);
      require(static_cast<bool>(services.isBatDisallowedGadget),
        "Agent disallowed-tools import required");
      Json gadgets=Json::array();
      for(const auto& gadget:filtered)
        if(!services.isBatDisallowedGadget(stringField(gadget,"name")))
          gadgets.push_back(gadget);
      gadgets.push_back(structured.shape);
      const auto systemText="You are verifying a stop condition in Batbox. "
        "Your task is to verify that the agent completed the given plan. "
        "The conversation transcript is available at: "+transcriptPath+
        "\nYou can read this file to analyze the conversation history if needed.\n\n"
        "Use the available tools to inspect the codebase and verify the condition.\n"
        "Use as few steps as possible - be efficient and direct.\n\n"
        "When done, return your result using the "+
        services.structuredOutputGadgetName+" tool with:\n"
        "- ok: true if the condition is met\n"
        "- ok: false with reason if the condition is not met";
      const auto& model=field(call.hook->fields,"model");
      std::string selected;
      if(!model.is_null())selected=model.get<std::string>();
      else{
        require(static_cast<bool>(services.getSmallFastModel),
          "Agent small-fast-model import required");
        selected=services.getSmallFastModel();
      }
      require(static_cast<bool>(services.randomUUID),
        "Agent hook UUID import required");
      hookBatId="hook-agent-"+services.randomUUID();
      require(static_cast<bool>(services.registerStructuredOutputEnforcement),
        "Agent structured-output enforcement import required");
      services.registerStructuredOutputEnforcement(hookBatId);
      BatQueryRequest request;
      request.messages=batMessages;
      request.systemPoetry=Json::array({systemText});
      request.gadgets=std::move(gadgets);
      request.batId=hookBatId;request.model=selected;
      request.signal=batAbort.get_token();
      request.sourceContext=call.context.sourceContext;
      request.indigenousStructuredOutputGadget=structured.indigenous;
      request.getAppState=[weak=weak_from_this()]{
        if(auto run=weak.lock())return run->batAppState();
        throw std::runtime_error("Agent hook context expired");};
      request.setInProgressGadgetUseIDs=[]{};
      require(static_cast<bool>(services.hasPermissionsToUseGadget),
        "Agent tool permission import required");
      request.canUseGadget=services.hasPermissionsToUseGadget;
      require(static_cast<bool>(services.query),
        "Agent query iterator import required");
      stream=services.query(std::move(request));
      nextMessage();
    }catch(...){
      if(parentTimeout)innerError(std::current_exception());
      else outerError(std::current_exception());
    }
  }
};
BatHookHost::BatHookHost(event_loop::Loop& loop,
  BatHookServices services):state_(std::make_shared<State>(loop,
    std::move(services))){}
event_loop::Deferred<HookExecutionResult> BatHookHost::execute(
  BatHookCall call){
  auto& state=*state_;
  if(!state.loop.isOwnerThread())
    throw std::logic_error("Agent hook owner required");
  const auto id=state.nextId++;
  auto run=std::make_shared<Run>(state_,std::move(call),id);
  state.active.emplace(id,run);
  auto output=run->output;run->start();return output;
}
std::size_t BatHookHost::activeCount()const{
  if(!state_->loop.isOwnerThread())
    throw std::logic_error("Agent hook owner required");
  return state_->active.size();
}
} // namespace batbox::instructions::hooks::runtime
