#include "application_bats/IndigenousBatApplication.hpp"
#include "application_bats/GeneralPurpose.hpp"
#include "application_bats/Definitions.hpp"
#include "application/Async.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include "bats/IndigenousMissionOutput.hpp"
#include "flights/ProjectDirectories.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* name){return value::property(value,name);}
}
std::shared_ptr<IndigenousBatApplication> IndigenousBatApplication::create(Inputs inputs){
  auto result=std::shared_ptr<IndigenousBatApplication>(
    new IndigenousBatApplication(std::move(inputs)));
  result->initialize();return result;
}
IndigenousBatApplication::~IndigenousBatApplication(){
  std::vector<std::function<void()>> pending;
  {std::lock_guard lock(cleanupMutex_);
    for(auto& [_,callback]:cleanups_)pending.push_back(std::move(callback));
    cleanups_.clear();}
  for(auto& callback:pending)if(callback)try{callback();}catch(...){}
  if(writes_)try{writes_->flush();}catch(...){}
}
void IndigenousBatApplication::initialize(){
  if(!inputs_.store||!inputs_.engine||!inputs_.model||!inputs_.messages||!inputs_.permissions||
     !inputs_.currentGadgets||
     (!inputs_.getUserContext&&!inputs_.getUserContextAsync)||
     (!inputs_.getSystemContext&&!inputs_.getSystemContextAsync)||
     (!inputs_.getSystemPoetry&&!inputs_.getSystemPoetryAsync)||
     !inputs_.enhanceSystemPoetry||
     !inputs_.enqueueSdkEvent||!inputs_.enqueuePendingNotification||
     !inputs_.killShellMissionsForBat||!inputs_.notifyCommandLifecycle)
    throw std::invalid_argument("Native Agent application imports incomplete");
  if(inputs_.flightId.empty())throw std::invalid_argument("Agent session ID required");
  DefinitionInputs definitionInputs;
  definitionInputs.config=inputs_.config;
  definitionInputs.projectRoot=inputs_.projectRoot;
  definitionInputs.diagnostic=[this](const std::string& value){
    if(inputs_.logError)inputs_.logError(std::runtime_error(value));};
  definitions_=loadBatDefinitions(definitionInputs);
  const auto owner=inputs_.owner.dispatcher();
  if(!inputs_.currentFlightId)
    inputs_.currentFlightId=[id=inputs_.flightId]{return id;};
  const auto flightId=inputs_.currentFlightId;
  inputs_.currentFlightId=[owner,flightId]{
    return application::onOwnerAndWait<std::string>(owner,flightId);};
  const auto getGadgets=inputs_.currentGadgets;
  inputs_.currentGadgets=[owner,getGadgets]{
    return application::onOwnerAndWait<gadgets::Gadgets>(owner,getGadgets);
  };
  if(inputs_.getSystemPoetryAsync){
    const auto getPoetry=inputs_.getSystemPoetryAsync;
    inputs_.getSystemPoetry=[owner,getPoetry](const gadgets::Gadgets& gadgets,
        const Json& model,const Json& directories,const Json& clients){
      return application::awaitOnOwner<Json>(owner,
        [getPoetry,gadgets,model,directories,clients]{
          return getPoetry(gadgets,model,directories,clients);
        });
    };
  }else{
    const auto getPoetry=inputs_.getSystemPoetry;
    inputs_.getSystemPoetry=[owner,getPoetry](const gadgets::Gadgets& gadgets,
        const Json& model,const Json& directories,const Json& clients){
      return application::onOwnerAndWait<Json>(owner,
        [getPoetry,gadgets,model,directories,clients]{
          return getPoetry(gadgets,model,directories,clients);
        });
    };
  }
  const auto enhance=inputs_.enhanceSystemPoetry;
  inputs_.enhanceSystemPoetry=[owner,enhance](const Json& poetry,
      const std::string& model,const Json& directories,const Json& gadgets){
    return application::onOwnerAndWait<Json>(owner,
      [enhance,poetry,model,directories,gadgets]{
        return enhance(poetry,model,directories,gadgets);
      });
  };
  const auto sdk=inputs_.enqueueSdkEvent;
  inputs_.enqueueSdkEvent=[owner,sdk](const Json& event){
    application::onOwnerAndWait<void>(owner,[sdk,event]{sdk(event);});
  };
  const auto notification=inputs_.enqueuePendingNotification;
  inputs_.enqueuePendingNotification=[owner,notification](const Json& value){
    application::onOwnerAndWait<void>(owner,[notification,value]{notification(value);});
  };
  directories_=std::make_shared<flights::ProjectDirectories>(
    [home=inputs_.config.stateDirectory]{return runtime::pathToUtf8(home);});
  auto queueServices=flights::indigenousQueueServices();
  queueServices.onBackgroundError=[log=inputs_.logError](std::exception_ptr error){
    if(!log)return;
    try{if(error)std::rethrow_exception(error);}
    catch(const std::exception& value){log(value);}
    catch(...){log(std::runtime_error("Agent transcript write failed"));}
  };
  writes_=std::make_shared<flights::WriteQueue>(std::move(queueServices));
  const auto root=inputs_.engine;
  auto weak=weak_from_this();
  query_=std::make_shared<IndigenousQueryBridge>(inputs_.owner,
    [root,weak](const bats::BatContext& context,
        const IndigenousQueryBridge::CanUseGadget& canUse){
      if(auto self=weak.lock()){
        const auto& id=field(context.query.data,"agentId");
        if(id.is_string())self->rememberConversation(id.get<std::string>(),context,canUse);
      }
      auto child=root->forBat(context,canUse);
      return IndigenousQueryBinding{child->services(),child};
    },inputs_.notifyCommandLifecycle,[weak](const std::string& id,const Json& message){
      if(auto self=weak.lock())self->publishStream(id,message);
    });
  missionState_.getAppState=[weak]{
    if(auto self=weak.lock())return self->appState();
    throw std::logic_error("Agent state owner closed");
  };
  missionState_.setAppState=[weak](const auto& update){
    if(auto self=weak.lock())self->setAppState(update);
  };
  missionState_.nowMillis=platform::nowMillis;
  missionState_.sdkProgressSummariesEnabled=[]{return false;};
  missionState_.emitMissionProgress=[this](const Json& event){inputs_.enqueueSdkEvent(event);};
  // Mission output uses a durable JSONL symlink; there is no in-memory
  // mission-output stash to evict.
  missionState_.evictMissionOutput=[](const std::string&){};
  missionRuntime_.bind(missionState_);
  registration_.state=&missionState_;
  registration_.onRegistered=[this](const std::string& id,
      const bats::RegisteredLocalBat& mission){missionRuntime_.remember(id,mission);};
  registration_.getMissionOutputPath=[home=inputs_.config.stateDirectory](
      const std::string& id){return runtime::pathToUtf8(home/"tasks"/(id+".output"));};
  registration_.getBatTranscriptPath=[weak](const std::string& id){
    if(auto self=weak.lock())return self->transcriptPath(id);
    throw std::logic_error("Agent transcript owner closed");
  };
  registration_.initMissionOutputAsSymlink=[home=inputs_.config.stateDirectory,
      log=inputs_.logError](
      const std::string& id,const std::string& target){
    (void)bats::initializeMissionOutputAsSymlink(home/"tasks"/(id+".output"),
      runtime::pathFromUtf8(target),log);
  };
  registration_.registerCleanup=[weak](const std::function<void()>& callback){
    auto self=weak.lock();if(!self)throw std::logic_error("Agent cleanup owner closed");
    std::size_t key;
    {std::lock_guard lock(self->cleanupMutex_);
      key=++self->nextCleanup_;self->cleanups_[key]=callback;}
    return [weak,key]{if(auto self=weak.lock()){
      std::lock_guard lock(self->cleanupMutex_);self->cleanups_.erase(key);
    }};
  };
  registration_.enqueueSdkEvent=inputs_.enqueueSdkEvent;
  notification_.setAppState=missionState_.setAppState;
  notification_.abortSpeculation=[](const bats::RootSetAppState&){};
  notification_.getMissionOutputPath=registration_.getMissionOutputPath;
  notification_.enqueuePendingNotification=inputs_.enqueuePendingNotification;
  progress_.syntheticOutputGadgetName="SyntheticOutput";
  progress_.getGadgetSearchOrReadInfo=[](const std::string& name,
      const Json& input,const gadgets::Gadgets& gadgets){
    const auto* gadget=gadgets::findGadgetByName(gadgets,name);
    if(gadget&&gadget->isSearchOrReadCommand)return gadget->isSearchOrReadCommand(input);
    return Json::object();
  };
  foreground_.registration=&registration_;
  foreground_.runtime=&missionRuntime_;
  foreground_.signals=&foregroundSignals_;
}
Json IndigenousBatApplication::appState()const{
  return application::onOwnerAndWait<Json>(inputs_.owner.dispatcher(),
    [store=inputs_.store]{return store->getState()->snapshot();});
}
void IndigenousBatApplication::setAppState(
    const std::function<Json(const Json&)>& update)const{
  application::onOwnerAndWait<void>(inputs_.owner.dispatcher(),
    [store=inputs_.store,update]{
      store->setState([&](const app_state::AppStateRef& previous)
          ->app_state::AppStateRef{
        const Json before=previous->snapshot();
        const Json after=update(before);
        if(after==before)return previous;
        if(!after.is_object())throw std::invalid_argument("Agent state update must be an object");
        auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
        for(auto it=after.begin();it!=after.end();++it){
          if(!before.contains(it.key())||before[it.key()]!=it.value())
            next->fields->set(it.key(),it.value());
        }
        if(field(after,"toolPermissionContext")!=field(before,"toolPermissionContext"))
          next->permissionContext=std::make_shared<value::SharedRecord>(
            value::SharedRecord::fromJson(field(after,"toolPermissionContext")));
        if(field(after,"tasks")!=field(before,"tasks") && field(after,"tasks").is_object()){
          next->missions=previous->missions?
            std::make_shared<app_state::OrderedStringMap<value::ValueRef>>(*previous->missions):
            std::make_shared<app_state::OrderedStringMap<value::ValueRef>>();
          const auto prior=next->missions->entries();
          for(const auto& [name,_]:prior)if(!after["tasks"].contains(name))
            next->missions->erase(name);
          for(auto it=after["tasks"].begin();it!=after["tasks"].end();++it){
            const auto* current=next->missions->get(it.key());
            if(!current||!(*current)||**current!=it.value())
              next->missions->set(it.key(),std::make_shared<Json>(it.value()));
          }
        }
        if(field(after,"agentNameRegistry")!=field(before,"agentNameRegistry") &&
           field(after,"agentNameRegistry").is_array()){
          next->batNameRoster=
            std::make_shared<app_state::OrderedStringMap<std::string>>();
          for(const auto& entry:after["agentNameRegistry"])
            if(entry.is_array()&&entry.size()>1&&entry[0].is_string()&&
               entry[1].is_string())
              next->batNameRoster->set(entry[0].get<std::string>(),
                entry[1].get<std::string>());
        }
        return next;
      });
    });
}
std::string IndigenousBatApplication::transcriptPath(const std::string& id)const{
  auto paths=pathsForBat(id);
  std::lock_guard lock(pathsMutex_);return paths->getBatTranscriptPath(id);
}
std::string IndigenousBatApplication::parentFlightId(const std::string& id)const{
  (void)pathsForBat(id);
  std::lock_guard lock(pathsMutex_);return batFlights_.at(id);
}
void IndigenousBatApplication::abortMission(const std::string& id)const{
  if(const auto owner=missionRuntime_.abortOwner(id))owner->request_stop();
}
std::shared_ptr<flights::Paths> IndigenousBatApplication::pathsForBat(
    const std::string& id)const{
  const auto current=inputs_.currentFlightId();
  std::lock_guard lock(pathsMutex_);
  auto [bat,inserted]=batFlights_.try_emplace(id,current);
  (void)inserted;
  return pathsForFlightLocked(bat->second);
}
std::shared_ptr<flights::Paths> IndigenousBatApplication::pathsForFlightLocked(
    const std::string& flight)const{
  if(const auto existing=flightPaths_.find(flight);existing!=flightPaths_.end())
    return existing->second;
  if(flightPaths_.size()>=32)flightPaths_.erase(flightPaths_.begin());
  flights::PathServices services;
  services.getConfigHome=[home=inputs_.config.stateDirectory]{return runtime::pathToUtf8(home);};
  services.getFlightId=[flight]{return flight;};
  services.getOriginalCwd=[cwd=inputs_.config.cwd]{return runtime::pathToUtf8(cwd);};
  services.getFlightProjectDir=[]()->std::optional<std::string>{return {};};
  services.getProjectDir=[dirs=directories_](const std::string& cwd){
    return dirs->getProjectDir(cwd);};
  return flightPaths_.emplace(flight,
    std::make_shared<flights::Paths>(std::move(services))).first->second;
}
void IndigenousBatApplication::requireCapacity(std::stop_token stop){
  std::unique_lock lock(capacityMutex_);
  if(closing_||stop.stop_requested())throw std::runtime_error("Subagent launch interrupted");
  if(outstanding_>=inputs_.config.maxSubagents)
    throw std::runtime_error("Outstanding subagent limit reached");
  ++outstanding_;
  if(!capacityChanged_.wait(lock,stop,[this]{
      return closing_||active_<inputs_.config.maxConcurrentSubagents;})||closing_){
    --outstanding_;
    lock.unlock();notifyShutdownProgress();
    throw std::runtime_error("Subagent launch interrupted");
  }
  ++active_;
}
void IndigenousBatApplication::releaseCapacity(){
  {std::lock_guard lock(capacityMutex_);
    if(active_)--active_;
    if(outstanding_)--outstanding_;}
  capacityChanged_.notify_all();
  notifyShutdownProgress();
}
void IndigenousBatApplication::rollbackCapacity(){
  releaseCapacity();
}
Json IndigenousBatApplication::batDefinitions()const{
  std::lock_guard lock(definitionsMutex_);
  return definitions_;
}
void IndigenousBatApplication::reloadDefinitions(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent definition reload requires owner");
  DefinitionInputs input;input.config=inputs_.config;input.projectRoot=inputs_.projectRoot;
  input.diagnostic=[this](const std::string& value){if(inputs_.logError)inputs_.logError(std::runtime_error(value));};
  auto definitions=loadBatDefinitions(input);
  {std::lock_guard lock(definitionsMutex_);definitions_=definitions;}
  setAppState([definitions](const Json& previous){Json next=previous;next["agentDefinitions"]=definitions;return next;});
}
} // namespace batbox::application_bats
