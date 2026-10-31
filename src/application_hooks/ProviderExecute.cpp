#include "application_hooks/Provider.hpp"
#include "application/Async.hpp"
#include "instructions/hooks/runtime/ExecuteHooks.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Platform.hpp"
#include <stdexcept>

namespace batbox::application_hooks {
namespace {
std::string field(const Json& object,const char* key){
  const auto& value=value::property(object,key);
  return value.is_string()?value.get<std::string>():std::string{};
}
std::string display(const Json& hook){
  const auto status=field(hook,"statusMessage");
  if(!status.empty())return status;
  const auto type=field(hook,"type");
  if(type=="command")return field(hook,"command");
  if(type=="prompt"||type=="agent")return field(hook,"prompt");
  if(type=="http")return field(hook,"url");
  return type;
}
std::string errorText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "unknown hook error";}
  return {};
}
struct Collector:std::enable_shared_from_this<Collector>{
  std::shared_ptr<Provider> provider;
  hooks::HookYieldCursor cursor;
  event_loop::Deferred<std::vector<Json>> output;
  std::vector<Json> values;
  Collector(std::shared_ptr<Provider> owner,event_loop::Loop& loop):
    provider(std::move(owner)),output(loop){}
  void next(){
    auto self=shared_from_this();
    try{
      cursor.next().then([self](const hooks::HookYield* row,
          const std::exception_ptr& error){
        if(error){self->output.reject(error);return;}
        if(row->done){self->output.resolve(std::move(self->values));return;}
        self->values.push_back(row->value);
        self->next();
      });
    }catch(...){output.reject(std::current_exception());}
  }
};
}
hooks::HookYieldCursor Provider::makeSourceCursor(
    const std::string& event,Json detail,std::string matchQuery,
    std::stop_token signal,std::optional<execution::Context> queryContext,
    std::optional<Json> hookMessages){
  if(!owner_.isOwnerThread())throw std::logic_error("Hook provider owner required");
  if(detail.is_object()&&detail.contains("permission_mode")&&
      detail.at("permission_mode").is_string())
    detail.update(baseInput(detail.at("permission_mode").get<std::string>()));
  else detail.update(baseInput());
  detail["hook_event_name"]=event;
  settingsHooks(event);
  if(inputs_.getAppState)flightHooks(inputs_.getAppState(),event);
  const auto batId=field(detail,"agent_id");
  const auto selected=batId.empty()?baseServices_.getFlightId():batId;
  auto active=std::make_shared<IndigenousHookContext>();
  active->query=queryContext?*queryContext:
    inputs_.currentContext?inputs_.currentContext():execution::Context{};
  active->gadgets=queryContext?queryContext->gadgets:
    inputs_.currentGadgets?inputs_.currentGadgets():gadgets::Gadgets{};
  if(active->query.gadgets.empty())active->query.gadgets=active->gadgets;
  if(active->query.baseGadgets.empty())active->query.baseGadgets=active->gadgets;
  hooks::HookBatchContext context;
  context.getAppState=[get=inputs_.getAppState,query=active->query,
      effective=queryContext.has_value()||event=="Stop"]{
    if(!get)throw std::logic_error("Hook app-state provider is unavailable");
    auto state=get();
    return effective?execution::effectiveAppState(query,state):state;};
  if(inputs_.model)context.gadgets=inputs_.model->gadgetMetadata(active->gadgets);
  context.sourceContext=active;
  if(!batId.empty())context.batId=batId;
  const auto batType=field(detail,"agent_type");
  if(!batType.empty())context.batType=batType;

  hooks::ExecuteHooksInput call;
  call.batch.hookInput=detail;
  call.batch.gadgetUseID=field(detail,"tool_use_id");
  if(call.batch.gadgetUseID.empty())call.batch.gadgetUseID=inputs_.randomUUID();
  if(!matchQuery.empty())call.batch.matchQuery=std::move(matchQuery);
  call.batch.signal=signal.stop_possible()?signal:active->query.stop;
  // Lifecycle hooks have no GadgetUseContext. Preserve that
  // absence so a poetry/bat hook reports its own non-blocking error while
  // command and HTTP hooks in the same batch still run.
  if(queryContext||event=="UserPromptSubmit"||event=="Stop"||
      event=="SubagentStop")
    call.batch.gadgetUseContext=std::move(context);
  const auto poetryMessages=hookMessages;
  if(hookMessages)call.messages=std::move(hookMessages);
  else if(inputs_.currentMessages)call.messages=inputs_.currentMessages();

  hooks::ExecuteHooksServices services;
  services.defaultTimeoutMs=[this]{return inputs_.defaultTimeoutMs;};
  services.jsonStringify=[](const Json& value){return value::stringify(value);};
  services.logError=[log=inputs_.log](const std::string& message,
      std::exception_ptr error){log(message+": "+errorText(error));};
  services.batch.shouldDisableAllHooksIncludingManaged=
    executor_.allHooksDisabled;
  services.batch.isSimpleEnvironment=executor_.simpleMode;
  services.batch.shouldSkipHookDueToTrust=executor_.skipDueToTrust;
  services.batch.logForDebugging=inputs_.log;
  services.batch.getFlightId=baseServices_.getFlightId;
  services.batch.randomUUID=inputs_.randomUUID;
  services.batch.nowIso=[]{return platform::isoTimestamp();};
  services.batch.nowMillis=[]{return static_cast<double>(platform::nowMillis());};
  services.batch.addToTurnHookDuration=[](double){};
  services.batch.isInternalHook=[](const hooks::HookBatchMatch& row){
    return row.hook&&value::property(row.hook->fields,"type")=="callback"&&
      value::property(row.hook->fields,"internal")==true;};
  services.batch.getHookDisplayText=[](const hooks::HookBatchMatch& row){
    return row.hook?display(row.hook->fields):std::string{};};
  services.batch.getMatchingHooks=[this](const std::optional<Json>& state,
      const std::string& flight,const std::string& hookEvent,
      const Json& input,const std::optional<Json>&){
    event_loop::Deferred<std::vector<hooks::HookBatchMatch>> output(owner_);
    hooks::getMatchingHooks(owner_,state,flight,hookEvent,input,matcher_).then(
      [output](
          const std::vector<hooks::MatchedHook>* matches,
          const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        std::vector<hooks::HookBatchMatch> rows;rows.reserve(matches->size());
        for(const auto& match:*matches){
          hooks::HookBatchMatch row;
          row.hook=std::make_shared<hooks::HookIdentity>(
            hooks::HookIdentity{match.hook});
          row.function=match.function;
          row.pluginRoot=match.pluginRoot;row.pluginId=match.pluginId;
          row.skillRoot=match.skillRoot;row.hookSource=match.hookSource;
          rows.push_back(std::move(row));
        }
        output.resolve(std::move(rows));
      });
    return output;
  };

  services.perHook.processJson.createAttachmentMessage=
    [create=inputs_.createAttachmentMessage](const Json& value,
        const std::vector<std::string>&){return create(value);};
  services.perHook.functionHost=functionHost_.get();
  services.perHook.createAttachmentMessage=inputs_.createAttachmentMessage;
  services.perHook.randomUUID=inputs_.randomUUID;
  services.perHook.nowMillis=services.batch.nowMillis;
  services.perHook.getHookDisplayText=services.batch.getHookDisplayText;
  services.perHook.execCommandHook=[command=executor_.execCommandHook](
      hooks::HookCommandCall call){
    if(!command)throw std::logic_error("Command hook executor unavailable");
    return command(hooks::CommandRequest{call.hook,call.hookEvent,
      call.hookName,call.jsonInput,call.hookId,call.hookIndex,
      call.pluginRoot,call.pluginId,call.signal,call.skillRoot});};
  services.perHook.execHttpHook=[http=executor_.execHttpHook](
      hooks::HookHttpCall call){
    if(!http)throw std::logic_error("HTTP hook executor unavailable");
    return http(hooks::HttpRequest{call.hook,call.hookEvent,
      call.jsonInput,call.signal.value_or(std::stop_token{})});};
  services.perHook.execPoetryHook=[host=poetryHost_,poetryMessages](
      hooks::HookModelCall input){
    if(!host)throw std::logic_error("Prompt hook executor unavailable");
    hooks::PoetryHookCall call;
    call.hook=input.hookIdentity;call.hookName=input.hookName;
    call.hookEvent=input.hookEvent;call.jsonInput=input.jsonInput;
    call.gadgetUseID=input.gadgetUseID;call.signal=input.signal;
    call.context=std::move(input.gadgetUseContext);
    call.messages=poetryMessages;
    return host->execute(std::move(call));};
  services.perHook.execBatHook=[host=batHost_](
      hooks::HookModelCall input){
    if(!host)throw std::logic_error("Agent hook executor unavailable");
    if(!input.gadgetUseContext.sourceContext)
      throw std::logic_error("Agent hook native context unavailable");
    const auto shared=std::static_pointer_cast<IndigenousHookContext>(
      input.gadgetUseContext.sourceContext);
    auto invocation=std::make_shared<IndigenousHookContext>();
    invocation->gadgets=shared->gadgets;
    invocation->query=shared->query;
    input.gadgetUseContext.sourceContext=invocation;
    hooks::BatHookCall call;
    call.hook=input.hookIdentity;call.hookName=input.hookName;
    call.hookEvent=input.hookEvent;call.jsonInput=input.jsonInput;
    call.gadgetUseID=input.gadgetUseID;call.signal=input.signal;
    call.context=std::move(input.gadgetUseContext);
    call.messages=std::move(input.messages);
    call.batName=std::move(input.batType);
    return host->execute(std::move(call));};
  services.perHook.parseHookOutput=[parser=parser_](const std::string& raw){
    return hooks::parseHookOutput(raw,parser);};
  services.perHook.parseHttpHookOutput=[parser=parser_](const std::string& raw){
    return hooks::parseHttpHookOutput(raw,parser);};
  services.perHook.emitHookStarted=[](const std::string&,
      const std::string&,const std::string&){};
  services.perHook.emitHookResponse=[emit=inputs_.emitHookEvent](const Json& row){
    if(emit){Json event=row;event["type"]="response";emit(event);}};
  services.perHook.emitHookResponseWithUndefined=
    [emit=inputs_.emitHookEvent](const Json& row,
        const std::vector<std::string>&){
      if(emit){Json event=row;event["type"]="response";emit(event);}};
  services.perHook.bold=[](const std::string& value){return value;};
  services.perHook.errorMessage=errorText;

  services.results.getHookDisplayText=[](const hooks::HookIdentity& hook){
    return display(hook.fields);};
  services.results.logForDebugging=inputs_.log;
  services.results.createAttachmentMessage=inputs_.createAttachmentMessage;
  services.results.getFlightId=[selected]{return selected;};
  services.results.getFlightHookCallback=[this](const Json&,
      const std::string& flight,const std::string& hookEvent,
      const std::string&,const std::shared_ptr<const hooks::HookIdentity>& hook)
      ->std::optional<std::function<void(
        const std::shared_ptr<const hooks::HookIdentity>&,
        const hooks::HookExecutionResult&)>>{
    if(!hook||!value::truthy(value::property(hook->fields,"_native_once")))return {};
    const auto key=field(hook->fields,"_native_session_key");
    if(key.empty())return {};
    return [this,flight,hookEvent,key](const auto&,const auto&){
      removeOneShot(flight,hookEvent,key);};};
  services.results.logError=services.logError;
  services.results.nowMillis=services.batch.nowMillis;
  services.results.addToTurnHookDuration=[](double){};

  auto executor=std::make_shared<hooks::ExecuteHooks>(owner_,std::move(call),
    std::move(services));
  hooks::HookYieldCursor cursor;
  cursor.next=[executor]{return executor->next();};
  cursor.returnValue=[executor](std::optional<Json> value){
    return executor->returnValue(std::move(value));};
  cursor.throwError=[executor](std::exception_ptr error){
    return executor->throwError(error);};
  return cursor;
}
event_loop::Deferred<std::vector<Json>> Provider::runSource(
    const std::string& event,Json detail,std::string matchQuery,
    std::stop_token signal,std::optional<execution::Context> queryContext,
    std::optional<Json> hookMessages){
  auto run=std::make_shared<Collector>(shared_from_this(),owner_);
  run->cursor=makeSourceCursor(event,std::move(detail),std::move(matchQuery),
    signal,std::move(queryContext),std::move(hookMessages));
  auto output=run->output;
  run->next();
  return output;
}
void Provider::streamSource(const std::string& event,Json detail,
    std::string matchQuery,std::stop_token signal,
    std::optional<execution::Context> queryContext,
    std::optional<Json> hookMessages,
    const std::function<bool(const Json&)>& sink){
  if(!sink)throw std::invalid_argument("Hook result sink required");
  const auto owner=owner_.dispatcher();
  if(owner.isOwnerThread())throw std::logic_error(
    "Streaming hooks requires a query worker");
  auto cursor=application::onOwnerAndWait<hooks::HookYieldCursor>(owner,
    [self=shared_from_this(),event,detail=std::move(detail),
        matchQuery=std::move(matchQuery),signal,
        queryContext=std::move(queryContext),
        hookMessages=std::move(hookMessages)]()mutable{
      return self->makeSourceCursor(event,std::move(detail),
        std::move(matchQuery),signal,std::move(queryContext),
        std::move(hookMessages));
    });
  while(true){
    const auto row=application::awaitOnOwner<hooks::HookYield>(owner,
      [cursor]{return cursor.next();});
    if(row.done)break;
    bool continueReading=false;
    try{continueReading=sink(row.value);}
    catch(...){
      const auto failure=std::current_exception();
      try{(void)application::awaitOnOwner<hooks::HookYield>(owner,
        [cursor]{return cursor.returnValue({});});}catch(...){}
      std::rethrow_exception(failure);
    }
    if(!continueReading){
      (void)application::awaitOnOwner<hooks::HookYield>(owner,
        [cursor]{return cursor.returnValue({});});
      break;
    }
  }
}
} // namespace batbox::application_hooks
