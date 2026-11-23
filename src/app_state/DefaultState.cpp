#include "app_state/State.hpp"
#include <stdexcept>

namespace batbox::app_state {
namespace {
void required(bool present,const char* name){
  if(!present)throw std::invalid_argument(name);
}
}
AppStateRef getDefaultAppState(const DefaultAppStateServices& services){
  required(bool(services.isPartner),"AppState teammate import required");
  required(bool(services.isPlanModeRequired),
    "AppState teammate plan-mode import required");
  required(bool(services.getInitialSettings),
    "AppState initial settings import required");
  required(bool(services.getEmptyGadgetPermissionContext),
    "AppState permission context import required");
  required(bool(services.createEmptyAttributionState),
    "AppState attribution import required");
  required(bool(services.shouldEnableThinkingByDefault),
    "AppState thinking default import required");

  // Resolve the lazy dependency and short circuit before constructing state.
  const bool plan=services.isPartner()&&services.isPlanModeRequired();
  auto state=std::make_shared<AppState>();
  state->fields=std::make_shared<value::SharedRecord>();
  auto& fields=*state->fields;
  auto set=[&](const char* key,Json value){fields.set(key,std::move(value));};
  auto undefined=[&](const char* key){fields.setRef(key,{});};

  Json settings=services.getInitialSettings();
  const Json initialEffort=value::property(settings,"effortLevel");
  if(settings.is_object()&&settings.contains("env"))
    state->settingsEnv=std::make_shared<Json>(settings.at("env"));
  set("settings",std::move(settings));
  state->missions=std::make_shared<OrderedStringMap<value::ValueRef>>();
  set("tasks",Json::object());
  state->batNameRoster=std::make_shared<OrderedStringMap<std::string>>();
  set("agentNameRegistry",Json::object());
  set("verbose",false);
  set("mainLoopModel",nullptr);
  set("mainLoopModelForSession",nullptr);
  Json permission=services.getEmptyGadgetPermissionContext();
  permission["mode"]=plan?"plan":"default";
  state->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson(permission));
  set("toolPermissionContext",std::move(permission));
  set("agentDefinitions",{{"activeAgents",Json::array()},
    {"allAgents",Json::array()}});
  state->trackedFiles=std::make_shared<runtime::OrderedStringSet>();
  set("fileHistory",{{"snapshots",Json::array()},
    {"trackedFiles",Json::object()},{"snapshotSequence",0}});
  set("attribution",services.createEmptyAttributionState());
  set("mcp",{{"clients",Json::array()},{"tools",Json::array()},
    {"commands",Json::array()},{"resources",Json::object()},
    {"pluginReconnectKey",0}});
  set("todos",Json::object());
  set("thinkingEnabled",services.shouldEnableThinkingByDefault());
  state->flightHooks=
    std::make_shared<instructions::hooks::runtime::FlightHooksState>();
  set("sessionHooks",Json::object());
  fields.setRef("speculation",services.idleSpeculationState);
  set("speculationSessionTimeSavedMs",0);
  if(initialEffort=="low"||initialEffort=="medium"||
      initialEffort=="high"||initialEffort=="max")
    set("effortValue",initialEffort);
  else undefined("effortValue");
  set("fastMode",false);
  return state;
}
} // namespace batbox::app_state
