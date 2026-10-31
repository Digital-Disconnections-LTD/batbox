#include "cli/permissions/PermissionRace.hpp"
#include "cli/permissions/PoetryHelpers.hpp"
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::cli::permissions {
namespace {
using StopCallback=std::stop_callback<std::function<void()>>;
std::string errorText(const std::exception_ptr& error){
  try{if(error)std::rethrow_exception(error);}
  catch(const structured::RequestAbortError&){return "AbortError";}
  catch(const std::exception& cause){return "Error: "+std::string(cause.what());}
  catch(...){return "unknown error";}
  return "unknown error";
}
struct State:std::enable_shared_from_this<State> {
  event_loop::Loop& loop;
  structured::RequestProducer& requests;
  structured::ControlResponses& control;
  const gadgets::Gadget& gadget;
  Json input;
  PermissionUseContext context;
  Json assistantMessage;
  std::string gadgetUseID;
  PermissionRaceServices services;
  PermissionPoetryCallback onPermissionPoetry;
  event_loop::Deferred<Json> output;
  std::stop_source localAbort;
  std::unique_ptr<StopCallback> parentListener;
  bool decided=false;
  bool waitingForSdk=false;
  State(event_loop::Loop& owner,structured::RequestProducer& producer,
      structured::ControlResponses& responses,const gadgets::Gadget& selected,
      Json original,PermissionUseContext use,Json assistant,std::string id,
      PermissionRaceServices bindings,PermissionPoetryCallback poetry):
    loop(owner),requests(producer),control(responses),gadget(selected),
    input(std::move(original)),context(std::move(use)),
    assistantMessage(std::move(assistant)),gadgetUseID(std::move(id)),
    services(std::move(bindings)),onPermissionPoetry(std::move(poetry)),
    output(loop){}
  void finish(const Json& decision){
    if(decided)return;
    decided=true;
    try{
      if(control.getPendingPermissionRequests().empty())
        services.notifyFlightStateChanged("running");
      parentListener.reset();
      output.resolve(decision);
    }catch(...){output.reject(std::current_exception());}
  }
  void failed(const std::exception_ptr& error){
    if(decided)return;
    try{
      const Json result={{"behavior","deny"},
        {"message","Tool permission request failed: "+errorText(error)},
        {"toolUseID",gadgetUseID}};
      finish(permissionPoetryGadgetResultToPermissionDecision(
        result,gadget,input,context.poetry));
    }catch(...){
      decided=true;
      parentListener.reset();
      output.reject(std::current_exception());
    }
  }
  void sdkResult(const Json* value,const std::exception_ptr& error){
    if(decided)return;
    if(error){failed(error);return;}
    try{
      finish(permissionPoetryGadgetResultToPermissionDecision(
        *value,gadget,input,context.poetry));
    }catch(...){failed(std::current_exception());}
  }
  void startPoetry(const Json& main){
    try{
      // Hooks start before the SDK poetry is constructed or sent.
      const auto hook=executePermissionRequestHooksForSDK(loop,gadget.name,
        gadgetUseID,input,context.hooks,
        value::property(main,"suggestions").is_null()?std::nullopt:
          std::optional<Json>(value::property(main,"suggestions")),
        services.hooks);
      const auto id=services.randomUUID();
      if(onPermissionPoetry)
        onPermissionPoetry(buildRequiresActionDetails(gadget,input,gadgetUseID,id));
      const auto reason=serializeDecisionReason(
        value::property(main,"decisionReason"),services.feature);
      Json request={{"subtype","can_use_tool"},{"tool_name",gadget.name},
        {"input",input},{"tool_use_id",gadgetUseID}};
      if(const auto& value=value::property(main,"suggestions");!value.is_null())
        request["permission_suggestions"]=value;
      if(const auto& value=value::property(main,"blockedPath");!value.is_null())
        request["blocked_path"]=value;
      if(reason)request["decision_reason"]=*reason;
      if(context.batId)request["agent_id"]=*context.batId;
      const auto sdk=requests.sendRequest(request,
        services.parsePermissionGadgetOutput,localAbort.get_token(),id);
      const auto self=shared_from_this();
      // Both callbacks are registered in race order. A hook
      // passthrough leaves the SDK request active and awaits its settlement.
      hook.then([self](const std::optional<Json>* value,
                     const std::exception_ptr& error){
        if(self->decided||self->waitingForSdk)return;
        if(error){self->failed(error);return;}
        if(value&&*value){
          self->localAbort.request_stop();
          self->finish(**value);
        }else self->waitingForSdk=true;
      });
      sdk.then([self](const Json* value,const std::exception_ptr& error){
        self->sdkResult(value,error);
      });
    }catch(...){failed(std::current_exception());}
  }
  void start(const std::optional<Json>& forceDecision){
    try{
      if(forceDecision){acceptMain(*forceDecision);return;}
      if(!services.hasPermissionsToUseGadget)
        throw std::invalid_argument("Permission checker import required");
      const auto pending=services.hasPermissionsToUseGadget(gadget,input,context,
        assistantMessage,gadgetUseID);
      const auto self=shared_from_this();
      pending.then([self](const Json* value,const std::exception_ptr& error){
        // A failed main check propagates without poetry cleanup.
        if(error){self->output.reject(error);return;}
        self->acceptMain(*value);
      });
    }catch(...){output.reject(std::current_exception());}
  }
  void acceptMain(const Json& decision){
    try{
      const auto& behavior=value::property(decision,"behavior");
      if(behavior=="allow"||behavior=="deny"){
        output.resolve(decision);return;
      }
      if(!services.notifyFlightStateChanged||!services.randomUUID||
         !services.parsePermissionGadgetOutput||!services.feature)
        throw std::invalid_argument("Permission race imports required");
      // The parent listener is added before the hook and poetry begin.
      if(context.parentAbort.stop_possible()&&
         !context.parentAbort.stop_requested()){
        const std::weak_ptr<State> weak=shared_from_this();
        parentListener=std::make_unique<StopCallback>(context.parentAbort,
          std::function<void()>([weak]{
            if(const auto state=weak.lock())state->localAbort.request_stop();
          }));
      }
      startPoetry(decision);
    }catch(...){failed(std::current_exception());}
  }
};
}
event_loop::Deferred<Json> createCanUseGadget(
    event_loop::Loop& loop,structured::RequestProducer& requests,
    structured::ControlResponses& control,const gadgets::Gadget& gadget,
    const Json& input,const PermissionUseContext& context,
    const Json& assistantMessage,const std::string& gadgetUseID,
    const std::optional<Json>& forceDecision,
    const PermissionRaceServices& services,
    const PermissionPoetryCallback& onPermissionPoetry){
  auto state=std::make_shared<State>(loop,requests,control,gadget,input,context,
    assistantMessage,gadgetUseID,services,onPermissionPoetry);
  state->start(forceDecision);
  return state->output;
}
}
