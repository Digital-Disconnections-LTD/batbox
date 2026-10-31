#include "bats/TypedBatState.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& value,const char* name){return value::property(value,name);}
}
std::function<app_state::AppStateRef()> effectiveTypedBatGetAppState(
    const BatContext& context){
  if(!context.getTypedAppState)return {};
  if(!context.query.typedAppStateModifierComplete)return {};
  if(context.query.appStateModifier&&!context.query.typedAppStateModifier)
    return {};
  if(!context.query.typedAppStateModifier)return context.getTypedAppState;
  const auto base=context.getTypedAppState;
  const auto modifier=context.query.typedAppStateModifier;
  return [base,modifier]{return modifier(base());};
}
std::function<app_state::AppStateRef()> createTypedBatGetAppState(
    std::function<app_state::AppStateRef()> parentGetAppState,
    Json batDefinition,bool isAsync,
    std::optional<bool> canShowPermissionPoetryEntries,
    std::optional<Json> allowedGadgets,
    std::function<bool(const std::string&)> feature){
  if(!parentGetAppState)throw std::invalid_argument("Typed parent app state required");
  return [parentGetAppState=std::move(parentGetAppState),
      batDefinition=std::move(batDefinition),isAsync,
      canShowPermissionPoetryEntries,allowedGadgets=std::move(allowedGadgets),
      feature=std::move(feature)]{
    const auto state=parentGetAppState();
    if(!state||!state->permissionContext)
      throw std::logic_error("Typed permission context required");
    const auto original=state->permissionContext;
    auto permission=original;
    const auto batMode=field(batDefinition,"permissionMode");
    const auto mode=original->get("mode");
    const auto currentMode=mode?*mode:Json(Json::value_t::discarded);
    auto updatePermission=[&]{
      if(permission==original)
        permission=std::make_shared<value::SharedRecord>(original->shallowCopy());
    };
    if(value::truthy(batMode)&&currentMode!="bypassPermissions"&&
       currentMode!="acceptEdits"){
      if(!feature)throw std::invalid_argument("Transcript classifier feature required");
      if(!(feature("TRANSCRIPT_CLASSIFIER")&&currentMode=="auto")){
        updatePermission();permission->set("mode",batMode);
      }
    }
    const bool avoid=canShowPermissionPoetryEntries?!*canShowPermissionPoetryEntries:
      batMode=="bubble"?false:isAsync;
    if(avoid){updatePermission();permission->set("shouldAvoidPermissionPrompts",true);}
    if(isAsync&&!avoid){
      updatePermission();permission->set("awaitAutomatedChecksBeforeDialog",true);
    }
    if(allowedGadgets){
      const auto rules=original->get("alwaysAllowRules");
      if(!rules||!rules->is_object())
        throw std::logic_error("Permission alwaysAllowRules required");
      const auto& cliArg=field(*rules,"cliArg");
      updatePermission();
      permission->set("alwaysAllowRules",Json{{"cliArg",cliArg},
        {"session",*allowedGadgets}});
    }
    const auto priorEffort=state->fieldRef("effortValue");
    auto effort=priorEffort;
    if(batDefinition.is_object()&&batDefinition.contains("effort")&&
       !batDefinition.at("effort").is_discarded())
      effort=std::make_shared<Json>(batDefinition.at("effort"));
    const bool sameEffort=effort==priorEffort||
      (effort&&priorEffort&&*effort==*priorEffort);
    if(permission==original&&sameEffort)return state;
    auto next=std::make_shared<app_state::AppState>(state->shallowCopy());
    if(permission!=original)next->setPermissionContext(permission);
    next->fields->setRef("effortValue",sameEffort?priorEffort:effort);
    return next;
  };
}
} // namespace batbox::bats
