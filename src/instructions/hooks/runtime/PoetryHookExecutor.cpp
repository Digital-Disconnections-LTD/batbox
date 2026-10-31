#include "instructions/hooks/runtime/PoetryHookExecutor.hpp"
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
constexpr const char* kPoetrySystemText=
  "You are evaluating a hook in Batbox.\n\n"
  "Your response must be a JSON object matching one of the following schemas:\n"
  "1. If the condition is met, return: {\"ok\": true}\n"
  "2. If the condition is not met, return: {\"ok\": false, \"reason\": \"Reason for why it is not met\"}";
}
struct PoetryHookHost::State {
  event_loop::Loop& loop;
  PoetryHookServices services;
  std::map<std::size_t,std::shared_ptr<Run>> active;
  std::size_t nextId=1;
  State(event_loop::Loop& owner,PoetryHookServices imports):loop(owner),
    services(std::move(imports)){}
};
struct PoetryHookHost::Run:std::enable_shared_from_this<Run>{
  std::weak_ptr<State> host;
  event_loop::Loop& loop;
  PoetryHookServices services;
  PoetryHookCall call;
  event_loop::Deferred<HookExecutionResult> output;
  std::shared_ptr<CombinedAbort> combined;
  std::size_t id;
  std::string effectiveGadgetUseID;
  bool finished=false;
  Run(const std::shared_ptr<State>& owner,PoetryHookCall input,
    std::size_t key):host(owner),loop(owner->loop),services(owner->services),
    call(std::move(input)),output(loop),id(key){}
  void require(bool supplied,const char* name){
    if(!supplied)throw std::invalid_argument(name);
  }
  void debug(const std::string& text){
    require(static_cast<bool>(services.logForDebugging),
      "Prompt hook local debug import required");
    services.logForDebugging(text);
  }
  HookExecutionResult result(const std::string& outcome,Json fields={}){
    fields["outcome"]=outcome;
    return {std::move(fields),outcome,call.hook};
  }
  Json attachment(Json payload){
    require(static_cast<bool>(services.createAttachmentMessage),
      "Prompt hook attachment factory required");
    return services.createAttachmentMessage(payload);
  }
  Json attachmentBase(const char* type){
    return {{"type",type},{"hookName",call.hookName},
      {"toolUseID",effectiveGadgetUseID},{"hookEvent",call.hookEvent}};
  }
  void finish(HookExecutionResult value){
    if(finished)return;
    finished=true;
    if(combined)combined->cleanup();
    output.resolve(std::move(value));
    if(auto state=host.lock())state->active.erase(id);
  }
  void fail(std::exception_ptr error){
    if(finished)return;
    finished=true;
    if(combined)combined->cleanup();
    output.reject(error);
    if(auto state=host.lock())state->active.erase(id);
  }
  void outerError(std::exception_ptr failure){
    try{
      require(static_cast<bool>(services.errorMessage),
        "Prompt hook error formatter required");
      const auto message=services.errorMessage(failure);
      debug("Hooks: Prompt hook error: "+message);
      auto payload=attachmentBase("hook_non_blocking_error");
      payload["stderr"]="Error executing prompt hook: "+message;
      payload["stdout"]="";payload["exitCode"]=1;
      finish(result("non_blocking_error",{{"message",attachment(payload)}}));
    }catch(...){fail(std::current_exception());}
  }
  void innerError(std::exception_ptr failure){
    try{
      if(combined)combined->cleanup();
      if(combined&&combined->aborted()){
        finish(result("cancelled"));return;
      }
      outerError(failure);
    }catch(...){outerError(std::current_exception());}
  }
  void onModel(const Json& response){
    try{
      combined->cleanup();
      require(static_cast<bool>(services.extractTextContent),
        "Prompt hook text extractor required");
      const auto& message=response.at("message");
      const auto content=services.extractTextContent(message.at("content"));
      require(static_cast<bool>(services.addResponseLength),
        "Prompt hook response-length import required");
      services.addResponseLength(value::stringLength(content));
      const auto fullResponse=value::trim(content);
      debug("Hooks: Model response: "+fullResponse);
      require(static_cast<bool>(services.safeParseJSON),
        "Prompt hook safe JSON parser required");
      const auto parsedJson=services.safeParseJSON(fullResponse);
      if(!parsedJson||!value::truthy(*parsedJson)){
        debug("Hooks: error parsing response as JSON: "+fullResponse);
        auto payload=attachmentBase("hook_non_blocking_error");
        payload["stderr"]="JSON validation failed";
        payload["stdout"]=fullResponse;payload["exitCode"]=1;
        finish(result("non_blocking_error",{{"message",attachment(payload)}}));
        return;
      }
      require(static_cast<bool>(services.schemaSafeParse),
        "Prompt hook response schema required");
      const auto parsed=services.schemaSafeParse(*parsedJson);
      if(!parsed.success){
        debug("Hooks: model response does not conform to expected schema: "+
          parsed.errorMessage);
        auto payload=attachmentBase("hook_non_blocking_error");
        payload["stderr"]="Schema validation failed: "+parsed.errorMessage;
        payload["stdout"]=fullResponse;payload["exitCode"]=1;
        finish(result("non_blocking_error",{{"message",attachment(payload)}}));
        return;
      }
      if(!value::truthy(field(parsed.data,"ok"))){
        const auto reason=parsed.data.contains("reason")?
          stringField(parsed.data,"reason"):std::string("undefined");
        debug("Hooks: Prompt hook condition was not met: "+reason);
        Json fields={{"blockingError",{{"blockingError",
          "Prompt hook condition was not met: "+reason},
          {"command",stringField(call.hook->fields,"prompt")}}},
          {"preventContinuation",true}};
        if(parsed.data.contains("reason"))
          fields["stopReason"]=parsed.data.at("reason");
        finish(result("blocking",std::move(fields)));return;
      }
      debug("Hooks: Prompt hook condition was met");
      auto payload=attachmentBase("hook_success");
      payload["content"]="";
      finish(result("success",{{"message",attachment(payload)}}));
    }catch(...){innerError(std::current_exception());}
  }
  void start(){
    try{
      if(!call.hook)throw std::invalid_argument("Prompt hook required");
      if(call.gadgetUseID.empty()){
        require(static_cast<bool>(services.randomUUID),
          "Prompt hook UUID import required");
        effectiveGadgetUseID="hook-"+services.randomUUID();
      }else effectiveGadgetUseID=call.gadgetUseID;
    }catch(...){fail(std::current_exception());return;}
    try{
      require(static_cast<bool>(services.addArgumentsToPoetry),
        "Prompt argument substitution import required");
      const auto poetry=services.addArgumentsToPoetry(
        stringField(call.hook->fields,"prompt"),call.jsonInput);
      debug("Hooks: Processing prompt hook with prompt: "+poetry);
      require(static_cast<bool>(services.createUserMessage),
        "Prompt user-message factory required");
      const auto user=services.createUserMessage(poetry);
      Json messages=Json::array();
      if(call.messages&&call.messages->is_array()&&!call.messages->empty())
        for(const auto& message:*call.messages)messages.push_back(message);
      messages.push_back(user);
      debug("Hooks: Querying model with "+std::to_string(messages.size())+
        " messages");
      const auto& timeout=field(call.hook->fields,"timeout");
      combined=CombinedAbort::create(loop,call.signal,std::nullopt,
        value::truthy(timeout)?timeout.get<double>()*1000:30000);
      PoetryQueryRequest request;
      request.messages=std::move(messages);
      request.systemPoetry=Json::array({kPoetrySystemText});
      request.gadgets=call.context.gadgets;
      request.signal=combined->signal();
      request.sourceContext=call.context.sourceContext;
      request.options={{"isNonInteractiveSession",true},
        {"hasAppendSystemPrompt",false},{"agents",Json::array()},
        {"querySource","hook_prompt"},{"mcpTools",Json::array()}};
      if(call.hook->fields.contains("model")&&
        !call.hook->fields.at("model").is_null())
        request.options["model"]=call.hook->fields.at("model");
      else{
        require(static_cast<bool>(services.getSmallFastModel),
          "Prompt small-fast-model import required");
        request.options["model"]=services.getSmallFastModel();
      }
      request.options["outputFormat"]={{"type","json_schema"},
        {"schema",{{"type","object"},
          {"properties",{{"ok",{{"type","boolean"}}},
            {"reason",{{"type","string"}}}}},
          {"required",Json::array({"ok"})},
          {"additionalProperties",false}}}};
      request.ownUndefinedKeys.push_back("options.toolChoice");
      if(call.context.batId)request.options["agentId"]=*call.context.batId;
      else request.ownUndefinedKeys.push_back("options.agentId");
      const auto context=call.context;
      auto& owner=loop;
      request.getGadgetPermissionContext=[context,&owner]{
        event_loop::Deferred<Json> permission(owner);
        try{
          if(!context.getAppState)
            throw std::invalid_argument("Prompt app-state getter required");
          const auto state=context.getAppState();
          permission.resolve(state.at("toolPermissionContext"));
        }catch(...){permission.reject(std::current_exception());}
        return permission;};
      require(static_cast<bool>(services.queryModelWithoutStreaming),
        "Prompt model query import required");
      const auto weak=weak_from_this();
      services.queryModelWithoutStreaming(std::move(request)).then(
        [weak](const Json* response,const std::exception_ptr& failure){
          if(auto run=weak.lock()){
            if(failure)run->innerError(failure);
            else run->onModel(*response);
          }
        });
    }catch(...){
      if(combined)innerError(std::current_exception());
      else outerError(std::current_exception());
    }
  }
};
PoetryHookHost::PoetryHookHost(event_loop::Loop& loop,
  PoetryHookServices services):state_(std::make_shared<State>(loop,
    std::move(services))){}
event_loop::Deferred<HookExecutionResult> PoetryHookHost::execute(
  PoetryHookCall call){
  auto& state=*state_;
  if(!state.loop.isOwnerThread())
    throw std::logic_error("Prompt hook owner required");
  const auto id=state.nextId++;
  auto run=std::make_shared<Run>(state_,std::move(call),id);
  state.active.emplace(id,run);
  auto output=run->output;
  run->start();
  return output;
}
std::size_t PoetryHookHost::activeCount()const{
  if(!state_->loop.isOwnerThread())
    throw std::logic_error("Prompt hook owner required");
  return state_->active.size();
}
} // namespace batbox::instructions::hooks::runtime
