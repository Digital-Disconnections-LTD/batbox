#include "cli/permissions/HookDecision.hpp"
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::cli::permissions {
namespace {
using Cursor=instructions::hooks::runtime::HookYieldCursor;
using Yield=instructions::hooks::runtime::HookYield;
struct State:std::enable_shared_from_this<State> {
  event_loop::Loop& loop;
  Json input;
  SdkHookContext context;
  HookDecisionServices services;
  event_loop::Deferred<std::optional<Json>> output;
  Cursor hooks;
  State(event_loop::Loop& owner,Json original,SdkHookContext ctx,
      HookDecisionServices bindings):loop(owner),input(std::move(original)),
      context(std::move(ctx)),services(std::move(bindings)),output(loop){}
  void next(){
    try{
      const auto self=shared_from_this();
      hooks.next().then([self](const Yield* value,
                             const std::exception_ptr& error){
        if(error){self->output.reject(error);return;}
        try{self->accept(*value);}catch(...){
          self->output.reject(std::current_exception());
        }
      });
    }catch(...){output.reject(std::current_exception());}
  }
  void accept(const Yield& yield){
    if(yield.done){output.resolve({});return;}
    const auto& decision=value::property(yield.value,"permissionRequestResult");
    const auto& behavior=value::property(decision,"behavior");
    if(value::truthy(decision)&&(behavior=="allow"||behavior=="deny")){
      if(behavior=="allow"){
        const auto& offeredInput=value::property(decision,"updatedInput");
        const Json finalInput=value::truthy(offeredInput)?offeredInput:input;
        const auto& offeredUpdates=value::property(decision,"updatedPermissions");
        const Json updates=offeredUpdates.is_null()?Json::array():
          offeredUpdates;
        if(updates.is_array()&&!updates.empty()){
          services.persistPermissionUpdates(updates);
          const auto current=context.getAppState();
          const auto updated=services.applyPermissionUpdates(
            current->permissionContext,updates);
          context.setAppState([updated](const app_state::AppStateRef& previous){
            if(previous->permissionContext==updated)return previous;
            auto next=std::make_shared<app_state::AppState>(
              previous->shallowCopy());
            next->setPermissionContext(updated);
            return next;
          });
        }
        output.resolve(Json{{"behavior","allow"},
          {"updatedInput",finalInput},{"userModified",false},
          {"decisionReason",Json{{"type","hook"},
            {"hookName","PermissionRequest"}}}});
      }else{
        const auto& offered=value::property(decision,"message");
        const Json message=value::truthy(offered)?offered:
          Json("Permission denied by PermissionRequest hook");
        output.resolve(Json{{"behavior","deny"},{"message",message},
          {"decisionReason",Json{{"type","hook"},
            {"hookName","PermissionRequest"}}}});
      }
      return;
    }
    next();
  }
};
}
event_loop::Deferred<std::optional<Json>> executePermissionRequestHooksForSDK(
    event_loop::Loop& loop,const std::string& gadgetName,
    const std::string& gadgetUseID,const Json& input,
    const SdkHookContext& context,const std::optional<Json>& suggestions,
    const HookDecisionServices& services){
  auto state=std::make_shared<State>(loop,input,context,services);
  try{
    if(!context.getAppState||!context.setAppState||
       !services.executePermissionRequestHooks||
       !services.persistPermissionUpdates||!services.applyPermissionUpdates)
      throw std::invalid_argument("Permission hook imports required");
    const auto appState=context.getAppState();
    const Json snapshot=appState->snapshot();
    const auto& mode=value::property(
      value::property(snapshot,"toolPermissionContext"),"mode");
    if(!mode.is_string())throw std::invalid_argument("Permission mode required");
    state->hooks=services.executePermissionRequestHooks(gadgetName,gadgetUseID,
      input,context.hookContext,mode.get<std::string>(),suggestions,
      context.abortToken);
    if(!state->hooks.next)throw std::invalid_argument("Hook cursor required");
    state->next();
  }catch(...){state->output.reject(std::current_exception());}
  return state->output;
}
}
