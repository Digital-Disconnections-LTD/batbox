#include "interactive/background/FlightBackgrounding.hpp"
#include <stdexcept>

namespace batbox::interactive::background {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Flight backgrounding binding required: ")+name);
}
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::optional<std::string> foregroundId(const app_state::AppStateRef& state){
  if(!state)return {};
  const auto value=state->fieldRef("foregroundedTaskId");
  return value&&value->is_string()?std::optional<std::string>(
    value->get<std::string>()):std::nullopt;
}
Json missionsSnapshot(const app_state::OrderedStringMap<value::ValueRef>& missions){
  Json value=Json::object();
  for(const auto& [id,mission]:missions.entries())if(mission)value[id]=*mission;
  return value;
}
}
FlightBackgrounding::FlightBackgrounding(event_loop::Loop& owner,
    FlightBackgroundingServices services):owner_(owner),
      services_(std::move(services)){need(services_.store,"store");}
FlightBackgrounding::~FlightBackgrounding(){if(unsubscribe_)unsubscribe_();}
void FlightBackgrounding::start(){
  if(!owner_.isOwnerThread())throw std::logic_error("Backgrounding owner required");
  if(unsubscribe_)return;
  auto weak=weak_from_this();
  unsubscribe_=services_.store->subscribe(
    std::make_shared<app_state::AppStateStore::Listener>([weak]{
      if(auto self=weak.lock())self->scheduleRefresh();
    }));
  scheduleRefresh();
}
void FlightBackgrounding::scheduleRefresh(){
  if(refreshQueued_)return;
  refreshQueued_=true;
  auto weak=weak_from_this();
  owner_.queueMicrotask([weak]{
    if(auto self=weak.lock()){
      self->refreshQueued_=false;
      self->refresh();
    }
  });
}
void FlightBackgrounding::clearForegroundId(){
  services_.store->setState([](const app_state::AppStateRef& previous){
    if(!foregroundId(previous))return previous;
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->fields->setRef("foregroundedTaskId",{});
    return next;
  });
}
void FlightBackgrounding::backgroundForegroundMission(){
  services_.store->setState([](const app_state::AppStateRef& previous){
    const auto id=foregroundId(previous);
    if(!id)return previous;
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->fields->setRef("foregroundedTaskId",{});
    if(previous->missions){
      if(const auto* mission=previous->missions->get(*id);mission&&*mission){
        next->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>(
          *previous->missions);
        Json updated=**mission;updated["isBackgrounded"]=true;
        next->missions->set(*id,std::make_shared<Json>(std::move(updated)));
        next->fields->set("tasks",missionsSnapshot(*next->missions));
      }
    }
    return next;
  });
}
void FlightBackgrounding::handleBackgroundFlight(){
  if(!owner_.isOwnerThread())throw std::logic_error("Backgrounding owner required");
  if(foregroundId(services_.store->getState())){
    backgroundForegroundMission();
    need(services_.setMessages,"setMessages");
    services_.setMessages(std::make_shared<TurnMessages>());
    need(services_.resetLoadingState,"resetLoadingState");
    services_.resetLoadingState();
    need(services_.setAbortController,"setAbortController");
    services_.setAbortController({});
    return;
  }
  need(services_.onBackgroundQuery,"onBackgroundQuery");
  services_.onBackgroundQuery();
}
void FlightBackgrounding::refresh(){
  if(!owner_.isOwnerThread())throw std::logic_error("Backgrounding owner required");
  const auto state=services_.store->getState();
  const auto id=foregroundId(state);
  if(!id){lastSyncedMessagesLength_=0;return;}
  const auto* mission=state->missions?state->missions->get(*id):nullptr;
  if(!mission||!*mission||field(**mission,"type")!="local_agent"){
    clearForegroundId();
    need(services_.resetLoadingState,"resetLoadingState");
    services_.resetLoadingState();
    lastSyncedMessagesLength_=0;
    return;
  }
  TurnMessagesRef messages;
  if(services_.messagesForMission)messages=services_.messagesForMission(*id,**mission);
  if(!messages&&services_.flightMissions){
    if(const auto sidecar=services_.flightMissions->find(*id))messages=sidecar->messages;
  }
  if(!messages){
    messages=std::make_shared<TurnMessages>();
    const auto& serialized=field(**mission,"messages");
    if(serialized.is_array())for(const auto& item:serialized)
      messages->push_back(std::make_shared<Json>(item));
  }
  if(messages->size()!=lastSyncedMessagesLength_){
    lastSyncedMessagesLength_=messages->size();
    need(services_.setMessages,"setMessages");
    services_.setMessages(std::make_shared<TurnMessages>(*messages));
  }
  if(field(**mission,"status")=="running"){
    std::shared_ptr<TurnAbort> abort;
    if(services_.abortForMission)abort=services_.abortForMission(*id,**mission);
    if(!abort&&services_.flightMissions){
      if(const auto sidecar=services_.flightMissions->find(*id))abort=sidecar->abort;
    }
    if(abort&&abort->aborted()){
      backgroundForegroundMission();
      need(services_.resetLoadingState,"resetLoadingState");
      services_.resetLoadingState();
      need(services_.setAbortController,"setAbortController");
      services_.setAbortController({});
      lastSyncedMessagesLength_=0;
      return;
    }
    need(services_.setIsLoading,"setIsLoading");
    services_.setIsLoading(true);
    if(abort){
      need(services_.setAbortController,"setAbortController");
      services_.setAbortController(std::move(abort));
    }
  }else{
    backgroundForegroundMission();
    need(services_.resetLoadingState,"resetLoadingState");
    services_.resetLoadingState();
    need(services_.setAbortController,"setAbortController");
    services_.setAbortController({});
    lastSyncedMessagesLength_=0;
  }
}
} // namespace batbox::interactive::background
