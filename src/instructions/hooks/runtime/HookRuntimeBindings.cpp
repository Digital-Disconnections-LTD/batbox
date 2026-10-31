#include "instructions/hooks/runtime/HookRuntimeBindings.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
std::function<BaseHookInput()> baseInput(BaseHookInputServices services){
  return [services=std::move(services)](){return createBaseHookInput(services);};
}
}
EnvHookServices bindEnvHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  std::shared_ptr<FlightEnvironment> flightEnvironment,
  double defaultTimeoutMs){
  if(!flightEnvironment)throw std::invalid_argument("Session environment required");
  EnvHookServices services;
  services.createBaseHookInput=baseInput(std::move(base));
  services.executeHooksOutsideRepl=[&loop,executor=std::move(executor)](
    HookDispatch dispatch){
    auto imports=executor;imports.getAppState={};
    return executeHooksOutsideRepl(loop,dispatch,imports);
  };
  services.invalidateFlightEnvStash=[environment=std::move(flightEnvironment)]{
    environment->invalidateFlightEnvStash();};
  services.defaultTimeoutMs=defaultTimeoutMs;
  return services;
}
EnvHookServices bindNotificationHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  double defaultTimeoutMs){
  EnvHookServices services;
  services.createBaseHookInput=baseInput(std::move(base));
  services.executeHooksOutsideRepl=[&loop,executor=std::move(executor)](
    HookDispatch dispatch){
    auto imports=executor;imports.getAppState={};
    return executeHooksOutsideRepl(loop,dispatch,imports);
  };
  services.defaultTimeoutMs=defaultTimeoutMs;
  return services;
}
CompactHookServices bindCompactHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,double defaultTimeoutMs){
  CompactHookServices services;
  services.createBaseHookInput=baseInput(std::move(base));
  services.executeHooksOutsideRepl=[&loop,executor=std::move(executor)](
    HookDispatch dispatch,std::optional<std::stop_token> signal){
    auto imports=executor;imports.getAppState={};
    return executeHooksOutsideRepl(loop,dispatch,imports,
      signal.value_or(std::stop_token{}));
  };
  services.defaultTimeoutMs=defaultTimeoutMs;
  return services;
}
FlightEndHookServices bindFlightEndHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  std::function<void(const SetAppState&,const std::string&)> clearFlightHooks,
  double defaultTimeoutMs){
  FlightEndHookServices services;
  services.getFlightId=base.getFlightId;
  services.createBaseHookInput=baseInput(std::move(base));
  services.executeHooksOutsideRepl=[&loop,executor=std::move(executor)](
    HookDispatch dispatch,const std::function<Json()>& getAppState,
    const std::optional<std::stop_token>& signal){
    auto imports=executor;imports.getAppState=getAppState;
    return executeHooksOutsideRepl(loop,dispatch,imports,
      signal.value_or(std::stop_token{}));
  };
  services.writeStderr=indigenousFlightEndStderrWrite;
  services.clearFlightHooks=std::move(clearFlightHooks);
  services.defaultTimeoutMs=defaultTimeoutMs;
  return services;
}
StopFailureServices bindStopFailureHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  HookConfigServices config,double defaultTimeoutMs){
  if(!config.getSnapshotHooks||!config.getRegisteredHooks||
    !config.getFlightHooks)
    throw std::invalid_argument("StopFailure hook config imports required");
  StopFailureServices services;
  services.getFlightId=base.getFlightId;
  services.createBaseHookInput=[base=std::move(base)](
    const StopFailureContext* context){
    std::optional<HookBatInfo> info;
    if(context)info=HookBatInfo{context->batId,context->batType};
    return createBaseHookInput(base,std::nullopt,std::nullopt,std::move(info));
  };
  services.presence.getHooksConfigFromSnapshot=[config]{
    const auto hooks=config.getSnapshotHooks("StopFailure");
    if(!hooks)return std::optional<Json>{};
    Json array=Json::array();
    for(std::size_t index=0;index<hooks->size();++index)
      array.push_back(Json::object());
    return std::optional<Json>(Json{{"StopFailure",std::move(array)}});
  };
  services.presence.getRegisteredHooks=[config]{
    const auto hooks=config.getRegisteredHooks("StopFailure");
    if(!hooks)return std::optional<Json>{};
    Json array=Json::array();
    for(std::size_t index=0;index<hooks->size();++index)
      array.push_back(Json::object());
    return std::optional<Json>(Json{{"StopFailure",std::move(array)}});
  };
  services.presence.hasFlightHook=[config](const std::optional<Json>& state,
    const std::string& flight,const std::string& event){
    return state&&config.getFlightHooks(*state,flight,event).has_value();
  };
  services.executeHooksOutsideRepl=[&loop,executor=std::move(executor)](
    HookDispatch dispatch,const std::function<Json()>& getAppState){
    auto imports=executor;imports.getAppState=getAppState;
    return executeHooksOutsideRepl(loop,dispatch,imports);
  };
  services.defaultTimeoutMs=defaultTimeoutMs;
  return services;
}
WorktreeHookServices bindWorktreeHookServices(event_loop::Loop& loop,
  WorktreeHookServices services,BaseHookInputServices base,
  ExecutorServices executor,double defaultTimeoutMs){
  services.createBaseHookInput=baseInput(std::move(base));
  services.executeHooksOutsideRepl=[&loop,executor=std::move(executor)](
    HookDispatch dispatch){
    auto imports=executor;imports.getAppState={};
    return executeHooksOutsideRepl(loop,dispatch,imports);
  };
  services.defaultTimeoutMs=defaultTimeoutMs;
  return services;
}
ElicitationHookServices bindElicitationHookServices(event_loop::Loop& loop,
  BaseHookInputServices base,ExecutorServices executor,
  HookSchemaImports schema,double defaultTimeoutMs){
  ElicitationHookServices services;
  services.createBaseHookInput=[base=std::move(base)](
    std::optional<std::string> permissionMode){
    return createBaseHookInput(base,std::move(permissionMode));
  };
  services.executeHooksOutsideRepl=[&loop,executor=std::move(executor)](
    HookDispatch dispatch,std::optional<std::stop_token> signal){
    auto imports=executor;imports.getAppState={};
    return executeHooksOutsideRepl(loop,dispatch,imports,
      signal.value_or(std::stop_token{}));
  };
  services.schemaSafeParse=[schema=std::move(schema)](const Json& input){
    return hookOutputSchemaSafeParse(input,schema);
  };
  services.defaultTimeoutMs=defaultTimeoutMs;
  return services;
}
void bindHookFlightEnvironment(IndigenousCommandConfig& command,
  AsyncHookServices& roster,std::shared_ptr<FlightEnvironment> environment){
  if(!environment)throw std::invalid_argument("Session environment required");
  command.getHookEnvFilePath=[environment](const std::string& event,
    std::size_t index){return environment->getHookEnvFilePath(event,index);};
  roster.invalidateFlightEnvStash=[environment]{
    environment->invalidateFlightEnvStash();};
}
} // namespace batbox::instructions::hooks::runtime
