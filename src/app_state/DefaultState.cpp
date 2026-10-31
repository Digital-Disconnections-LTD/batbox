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
  required(bool(services.shouldEnablePoetrySuggestion),
    "AppState prompt suggestion import required");
  required(bool(services.idleSpeculationState),
    "AppState idle speculation identity required");

  // Resolve the lazy dependency and short circuit before constructing state.
  const bool plan=services.isPartner()&&services.isPlanModeRequired();
  auto state=std::make_shared<AppState>();
  state->fields=std::make_shared<value::SharedRecord>();
  auto& fields=*state->fields;
  auto set=[&](const char* key,Json value){fields.set(key,std::move(value));};
  auto undefined=[&](const char* key){fields.setRef(key,{});};

  Json settings=services.getInitialSettings();
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
  undefined("statusLineText");
  set("expandedView","none");
  set("isBriefOnly",false);
  set("showTeammateMessagePreview",false);
  set("selectedIPAgentIndex",-1);
  set("coordinatorTaskIndex",-1);
  set("viewSelectionMode","none");
  set("footerSelection",nullptr);
  set("kairosEnabled",false);
  undefined("remoteSessionUrl");
  set("remoteConnectionStatus","connecting");
  set("remoteBackgroundTaskCount",0);
  set("replBridgeEnabled",false);
  set("replBridgeExplicit",false);
  set("replBridgeOutboundOnly",false);
  set("replBridgeConnected",false);
  set("replBridgeSessionActive",false);
  set("replBridgeReconnecting",false);
  undefined("replBridgeConnectUrl");
  undefined("replBridgeSessionUrl");
  undefined("replBridgeEnvironmentId");
  undefined("replBridgeSessionId");
  undefined("replBridgeError");
  undefined("replBridgeInitialName");
  set("showRemoteCallout",false);
  Json permission=services.getEmptyGadgetPermissionContext();
  permission["mode"]=plan?"plan":"default";
  state->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson(permission));
  set("toolPermissionContext",std::move(permission));
  undefined("agent");
  set("agentDefinitions",{{"activeAgents",Json::array()},
    {"allAgents",Json::array()}});
  state->trackedFiles=std::make_shared<runtime::OrderedStringSet>();
  set("fileHistory",{{"snapshots",Json::array()},
    {"trackedFiles",Json::object()},{"snapshotSequence",0}});
  set("attribution",services.createEmptyAttributionState());
  set("mcp",{{"clients",Json::array()},{"tools",Json::array()},
    {"commands",Json::array()},{"resources",Json::object()},
    {"pluginReconnectKey",0}});
  set("plugins",{{"enabled",Json::array()},{"disabled",Json::array()},
    {"commands",Json::array()},{"errors",Json::array()},
    {"installationStatus",{{"marketplaces",Json::array()},
      {"plugins",Json::array()}}},{"needsRefresh",false}});
  set("todos",Json::object());
  set("remoteAgentTaskSuggestions",Json::array());
  set("notifications",{{"current",nullptr},{"queue",Json::array()}});
  set("elicitation",{{"queue",Json::array()}});
  set("thinkingEnabled",services.shouldEnableThinkingByDefault());
  set("promptSuggestionEnabled",services.shouldEnablePoetrySuggestion());
  state->flightHooks=
    std::make_shared<instructions::hooks::runtime::FlightHooksState>();
  set("sessionHooks",Json::object());
  set("inbox",{{"messages",Json::array()}});
  set("workerSandboxPermissions",{{"queue",Json::array()},
    {"selectedIndex",0}});
  set("pendingWorkerRequest",nullptr);
  set("pendingSandboxRequest",nullptr);
  set("promptSuggestion",{{"text",nullptr},{"promptId",nullptr},
    {"shownAt",0},{"acceptedAt",0},{"generationRequestId",nullptr}});
  fields.setRef("speculation",services.idleSpeculationState);
  set("speculationSessionTimeSavedMs",0);
  set("skillImprovement",{{"suggestion",nullptr}});
  set("authVersion",0);
  set("initialMessage",nullptr);
  undefined("effortValue");
  state->activeOverlays=std::make_shared<runtime::OrderedStringSet>();
  set("activeOverlays",Json::object());
  set("fastMode",false);
  return state;
}
} // namespace batbox::app_state
