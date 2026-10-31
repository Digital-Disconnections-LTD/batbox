#include "application_hooks/Provider.hpp"
#include "application_hooks/PoetryArguments.hpp"
#include "application_permissions/PermissionProvider.hpp"
#include "bats/ForkedBat.hpp"
#include "instructions/hooks/runtime/BatOwnerQueryBinding.hpp"
#include "instructions/hooks/runtime/BatSyntheticGadgetBinding.hpp"
#include "instructions/hooks/runtime/HookResponseSchema.hpp"
#include "instructions/hooks/runtime/FlightHookBinding.hpp"
#include "interactive/StreamReducer.hpp"
#include "messages/Messages.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Platform.hpp"
#include "gadgets/Gadget.hpp"
#include <future>
#include <set>
#include <stdexcept>

namespace batbox::application_hooks {
namespace {
constexpr const char* kOutputGadget="StructuredOutput";
bool disallowed(const std::string& name){
  // Gadgets excluded from the default hook Bat profile.
  static const std::set<std::string> names={"TaskOutput","ExitPlanMode",
    "EnterPlanMode","Agent","AskUserQuestion","TaskStop"};
  return names.contains(name);
}
std::string errorText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "unknown agent hook error";}
  return {};
}
std::future<Json> futureFromDeferred(event_loop::Loop::Dispatcher owner,
    std::function<event_loop::Deferred<Json>(const Json&)> request,Json input){
  auto promise=std::make_shared<std::promise<Json>>();
  auto output=promise->get_future();
  auto invoke=[request=std::move(request),input=std::move(input),promise]{
    try{
      request(input).then([promise](const Json* value,
          const std::exception_ptr& error){
        if(error)promise->set_exception(error);
        else if(value)promise->set_value(*value);
        else promise->set_exception(std::make_exception_ptr(
          std::runtime_error("Agent hook permission result missing")));
      });
    }catch(...){promise->set_exception(std::current_exception());}
  };
  if(owner.isOwnerThread())invoke();
  else owner.postMission(std::move(invoke));
  return output;
}
std::shared_ptr<IndigenousHookContext> indigenousContext(
    const hooks::BatQueryRequest& request){
  if(!request.sourceContext)
    throw std::invalid_argument("Agent hook native context required");
  return std::static_pointer_cast<IndigenousHookContext>(request.sourceContext);
}
}
void Provider::bindBatHooks(){
  if(!inputs_.model)return;
  hooks::BatHookServices services;
  services.randomUUID=inputs_.randomUUID;
  services.getSmallFastModel=[model=inputs_.model]{
    return model->config().smallModel;};
  services.getTranscriptPath=[this]{
    if(!baseServices_.getTranscriptPathForFlight)
      throw std::logic_error("Agent hook transcript path import missing");
    return baseServices_.getTranscriptPathForFlight(
      baseServices_.getFlightId());};
  services.getBatTranscriptPath=inputs_.batTranscriptPath;
  services.addArgumentsToPoetry=hookPoetryArguments;
  services.logForDebugging=inputs_.log;
  services.createUserMessage=[messages=inputs_.messages](
      const std::string& poetry){
    if(!messages)throw std::logic_error("Agent hook message services missing");
    return messages::createUserMessage(Json{{"content",poetry}},*messages);
  };
  hooks::bindBatSyntheticOutputGadget(services,kOutputGadget);
  services.gadgetMatchesName=[](const Json& gadget,const std::string& name){
    return value::property(gadget,"name")==name;};
  services.isBatDisallowedGadget=disallowed;
  hooks::bindBatHookFlightStore(services,indigenousFlightStore_,kOutputGadget);
  const auto registerSourceStop=services.registerStructuredOutputEnforcement;
  services.registerStructuredOutputEnforcement=[this,registerSourceStop](
      const std::string& id){
    registerSourceStop(id);
    indigenousFlightStore_->addFunctionHook(id,"SubagentStop","",
      [](const Json& messages,std::stop_token)->hooks::FunctionHookReply{
        return hooks::hasSuccessfulGadgetCall(messages,kOutputGadget);},
      "You MUST call the StructuredOutput tool to complete this request. "
      "Call this tool now.",5000);
  };
  services.addResponseLength=inputs_.addResponseLength;
  services.setStreamMode=[](const Json&){};
  services.handleMessageFromStream=[](const Json& message,
      const std::function<void(std::size_t)>& add,
      const std::function<void(const Json&)>& setMode){
    interactive::StreamCallbacks callbacks;
    callbacks.onMessage=[](const Json&){};
    callbacks.onUpdateLength=[add](const std::string& text){
      add(value::stringLength(text));};
    callbacks.onSetStreamMode=[setMode](const std::string& value){
      if(setMode)setMode(value);};
    callbacks.onStreamingGadgetUses=[](interactive::GadgetUsesUpdate){};
    interactive::StreamImports imports;
    imports.connectorTextEnabled=[]{return false;};
    imports.isConnectorTextBlock=[](const Json&){return false;};
    imports.nowMs=[]{return static_cast<double>(platform::nowMillis());};
    interactive::handleMessageFromStream(message,callbacks,imports);
  };
  services.hasPermissionsToUseGadget=[this](const Json& input){
    event_loop::Deferred<Json> output(owner_);
    try{
      if(!inputs_.permission||!inputs_.getAppState)
        throw std::logic_error("Agent hook permission provider missing");
      auto state=inputs_.getAppState();
      auto context=value::property(state,"toolPermissionContext");
      if(!context.is_object())context=Json::object();
      context["mode"]="dontAsk";
      output.resolve(inputs_.permission->check("Agent",input,context).get());
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
  services.schemaSafeParse=hooks::hookResponseSchemaSafeParse;
  services.jsonStringify=[](const Json& value){return value::stringify(value);};
  services.createAttachmentMessage=inputs_.createAttachmentMessage;
  services.errorMessage=errorText;
  hooks::BatOwnerQueryBindings binding;
  binding.buildContext=[this](const hooks::BatQueryRequest& request){
    auto state=indigenousContext(request);
    auto query=hooks::makeBatHookQueryContext(request,state->query,
      disallowed,kOutputGadget);
    state->bat=std::make_shared<bats::BatContext>();
    state->bat->query=query;
    state->bat->getAppState=request.getAppState;
    state->bat->setAppState=inputs_.setAppState;
    state->bat->currentAbortReason=[stop=request.signal]{
      return stop.stop_requested()?std::string("interrupt"):std::string{};};
    return query;
  };
  binding.indigenousContext=[](const hooks::BatQueryRequest& request){
    auto state=indigenousContext(request);
    if(!state->bat)throw std::logic_error("Agent hook context not built");
    return state->bat;
  };
  binding.buildServices=[this](const hooks::BatQueryRequest& request){
    auto state=indigenousContext(request);
    if(!state->bat||!inputs_.makeBatEngine)
      throw std::logic_error("Agent hook query engine is unavailable");
    const auto owner=owner_.dispatcher();
    auto use=[owner,callback=request.canUseGadget](const Json& input){
      return futureFromDeferred(owner,callback,input);};
    state->child=inputs_.makeBatEngine(*state->bat,use);
    if(!state->child)throw std::logic_error(
      "Agent hook child query engine is unavailable");
    return *state->child;
  };
  hooks::bindBatOwnerQuery(owner_,services,std::move(binding));
  batHost_=std::make_shared<hooks::BatHookHost>(owner_,
    std::move(services));
}
} // namespace batbox::application_hooks
