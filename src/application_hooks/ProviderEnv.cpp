#include "application_hooks/Provider.hpp"
#include "instructions/hooks/runtime/EnvHooks.hpp"
#include "instructions/hooks/runtime/HookRuntimeBindings.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace batbox::application_hooks {
namespace {
hooks::EnvHookServices envServices(event_loop::Loop& owner,
    const hooks::BaseHookInputServices& base,
    const hooks::ExecutorServices& executor,
    const std::shared_ptr<hooks::FlightEnvironment>& environment,
    const std::function<Json()>& getAppState,double timeout){
  auto services=hooks::bindEnvHookServices(owner,base,executor,
    environment,timeout);
  services.executeHooksOutsideRepl=[&owner,executor,getAppState](
      instructions::hooks::HookDispatch dispatch){
    auto imports=executor;imports.getAppState=getAppState;
    return hooks::executeHooksOutsideRepl(owner,dispatch,imports);
  };
  return services;
}
}
event_loop::Deferred<hooks::EnvHookResult> Provider::fileChanged(
    const std::string& path,const std::string& event){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "FileChanged hook requires owner loop");
  auto services=envServices(owner_,baseServices_,executor_,
    flightEnvironment_,inputs_.getAppState,inputs_.defaultTimeoutMs);
  return hooks::executeFileChangedHooks(owner_,services,path,event);
}
event_loop::Deferred<hooks::EnvHookResult> Provider::cwdChanged(
    const std::string& oldCwd,const std::string& newCwd){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "CwdChanged hook requires owner loop");
  auto services=envServices(owner_,baseServices_,executor_,
    flightEnvironment_,inputs_.getAppState,inputs_.defaultTimeoutMs);
  return hooks::executeCwdChangedHooks(owner_,services,oldCwd,newCwd);
}
event_loop::Deferred<event_loop::Unit> Provider::clearCwdEnvFiles(){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "Session hook environment requires owner loop");
  return flightEnvironment_->clearCwdEnvFiles();
}
event_loop::Deferred<std::optional<std::string>> Provider::flightEnvironmentScript(){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "Session hook environment requires owner loop");
  // Each Flight has its own environment files; /clear and resume change it.
  const auto flightId=baseServices_.getFlightId();
  if(flightId!=environmentFlightId_){
    if(!environmentFlightId_.empty())flightEnvironment_->invalidateFlightEnvStash();
    environmentFlightId_=flightId;
  }
  // Without a parent file or a hook that wrote one, there is nothing to read.
  const char* parent=std::getenv("BATBOX_ENV_FILE");
  std::error_code error;
  if((!parent||!*parent)&&!std::filesystem::exists(
      inputs_.config.stateDirectory/"session-env"/runtime::pathFromUtf8(flightId),error)){
    event_loop::Deferred<std::optional<std::string>> none(owner_);
    none.resolve(std::nullopt);return none;
  }
  return flightEnvironment_->getFlightEnvironmentScript();
}
} // namespace batbox::application_hooks
