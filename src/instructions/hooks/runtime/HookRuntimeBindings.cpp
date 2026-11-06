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
} // namespace batbox::instructions::hooks::runtime
