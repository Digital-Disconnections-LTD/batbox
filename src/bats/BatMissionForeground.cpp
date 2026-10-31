#include "bats/BatMissionForeground.hpp"
#include <chrono>
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
template<class T>void need(const T& callback,const char* key){
  if(!callback)throw std::invalid_argument(std::string("Unbound foreground agent import: ")+key);
}
BatMissionStateServices& state(ForegroundServices& services){
  if(!services.registration||!services.registration->state)
    throw std::invalid_argument("Unbound foreground agent state");
  return *services.registration->state;
}
}
ObservedBackgroundSignal ForegroundSignals::createObserved(const std::string& id){
  auto promise=std::make_shared<std::promise<void>>();
  auto future=promise->get_future().share();
  auto completion=std::make_shared<CompletionSignal>();
  // BatGadget maps backgroundSignal with .then before entering the loop.
  completion->preRegisterMapping();
  std::lock_guard lock(mutex_);
  resolvers_[id]=Resolver{std::move(promise),completion};
  return {future,completion};
}
std::shared_future<void> ForegroundSignals::create(const std::string& id){
  return createObserved(id).future;
}
void ForegroundSignals::resolve(const std::string& id){
  Resolver resolver;
  {
    std::lock_guard lock(mutex_);
    const auto it=resolvers_.find(id);
    if(it==resolvers_.end())return;
    resolver=std::move(it->second);resolvers_.erase(it);
  }
  resolver.completion->settle();
  resolver.promise->set_value();
}
void ForegroundSignals::erase(const std::string& id){
  std::lock_guard lock(mutex_);
  const auto it=resolvers_.find(id);
  if(it==resolvers_.end())return;
  abandoned_.push_back(std::move(it->second));
  resolvers_.erase(it);
}
RegisteredForegroundBat registerBatForeground(
  const BatRegistrationParams& params,std::optional<double> autoBackgroundMs,
  ForegroundServices& services){
  if(!services.registration||!services.signals)
    throw std::invalid_argument("Unbound foreground agent registry");
  auto& imports=*services.registration;
  auto& missionState=state(services);
  need(imports.getBatTranscriptPath,"getAgentTranscriptPath");
  need(imports.initMissionOutputAsSymlink,"initTaskOutputAsSymlink");
  imports.initMissionOutputAsSymlink(params.batId,
    imports.getBatTranscriptPath(params.batId));
  RegisteredForegroundBat output;
  output.missionId=params.batId;
  if(imports.onCreateAbortOwner)imports.onCreateAbortOwner(false);
  output.registered.abortOwner=std::make_shared<std::stop_source>();
  need(imports.registerCleanup,"registerCleanup");
  output.registered.unregisterCleanup=imports.registerCleanup(
    [id=params.batId,state=&missionState]{killAsyncBat(id,*state);});
  Json mission=createMissionStateBase(params.batId,"local_agent",params.description,
    params.gadgetUseId,imports);
  const Json& batType=field(params.selectedBat,"agentType");
  mission["type"]="local_agent";
  mission["status"]="running";
  mission["agentId"]=params.batId;
  mission["prompt"]=params.poetry;
  mission["selectedAgent"]=params.selectedBat;
  mission["agentType"]=batType.is_string()?batType:Json("general-purpose");
  mission["abortController"]=Json::object();
  mission["retrieved"]=false;
  mission["lastReportedToolCount"]=0;
  mission["lastReportedTokenCount"]=0;
  mission["isBackgrounded"]=false;
  mission["pendingMessages"]=Json::array();
  mission["retain"]=false;
  mission["diskLoaded"]=false;
  output.registered.missionState=mission;
  const auto observed=services.signals->createObserved(params.batId);
  output.backgroundSignal=observed.future;
  output.backgroundCompletion=observed.completion;
  if(services.runtime)services.runtime->remember(params.batId,output.registered);
  registerMission(mission,imports);
  if(autoBackgroundMs&&*autoBackgroundMs>0){
    need(services.scheduleAutoBackground,"scheduleAutoBackground");
    output.cancelAutoBackground=services.scheduleAutoBackground(*autoBackgroundMs,
      [id=params.batId,&services]{
        auto& missionState=state(services);
        need(missionState.setAppState,"setAppState");
        missionState.setAppState([&](const Json& previous){
          const Json& prior=value::property(field(previous,"tasks"),id);
          if(!isLocalBatMission(prior)||value::truthy(field(prior,"isBackgrounded")))
            return previous;
          Json next=previous;next["tasks"][id]["isBackgrounded"]=true;
          return next;
        });
        services.signals->resolve(id);
      });
  }
  return output;
}
bool backgroundBatMission(const std::string& id,ForegroundServices& services){
  auto& missionState=state(services);
  need(missionState.getAppState,"getAppState");
  const Json current=missionState.getAppState();
  const Json& mission=value::property(field(current,"tasks"),id);
  if(!isLocalBatMission(mission)||value::truthy(field(mission,"isBackgrounded")))return false;
  need(missionState.setAppState,"setAppState");
  missionState.setAppState([&](const Json& previous){
    const Json& prior=value::property(field(previous,"tasks"),id);
    if(!isLocalBatMission(prior))return previous;
    Json next=previous;next["tasks"][id]["isBackgrounded"]=true;
    return next;
  });
  if(!services.signals)throw std::invalid_argument("Unbound foreground agent signals");
  services.signals->resolve(id);
  return true;
}
void unregisterBatForeground(const std::string& id,ForegroundServices& services){
  if(!services.signals)throw std::invalid_argument("Unbound foreground agent signals");
  services.signals->erase(id);
  auto& missionState=state(services);
  need(missionState.setAppState,"setAppState");
  bool removed=false;
  missionState.setAppState([&](const Json& previous){
    const Json& mission=value::property(field(previous,"tasks"),id);
    if(!isLocalBatMission(mission)||value::truthy(field(mission,"isBackgrounded")))
      return previous;
    Json next=previous;next["tasks"].erase(id);removed=true;return next;
  });
  if(removed){need(missionState.unregisterCleanup,"unregisterCleanup");
    missionState.unregisterCleanup(id);}
}
} // namespace batbox::bats
