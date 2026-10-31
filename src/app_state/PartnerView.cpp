#include "app_state/PartnerView.hpp"
#include <stdexcept>

namespace batbox::app_state {
namespace {
constexpr double panelGraceMs=30000;
value::ValueRef lookup(const AppStateRef& state,const std::string& id){
  if(!state->missions)return {};
  const auto value=state->missions->get(id);
  return value?*value:value::ValueRef{};
}
std::optional<std::string> viewedId(const AppStateRef& state){
  const auto value=state->fieldRef("viewingAgentTaskId");
  if(value&&value->is_string())return value->get<std::string>();
  return {};
}
bool retained(const value::ValueRef& mission){
  return mission&&value::truthy(value::property(*mission,"retain"));
}
Json snapshotMissions(const OrderedStringMap<value::ValueRef>& missions){
  Json out=Json::object();
  for(const auto& [id,mission]:missions.entries())
    out[id]=mission?*mission:Json(nullptr);
  return out;
}
void replaceMissions(AppState& state,
  std::shared_ptr<OrderedStringMap<value::ValueRef>> missions){
  state.fields->set("tasks",snapshotMissions(*missions));
  state.missions=std::move(missions);
}
}
bool isTerminalMissionStatus(const std::string& status){
  return status=="completed"||status=="failed"||status=="killed";
}
bool isLocalBat(const value::ValueRef& mission){
  return mission&&mission->is_object()&&
    value::property(*mission,"type")=="local_agent";
}
value::ValueRef releaseLocalBat(const value::ValueRef& mission,
  const PartnerViewServices& services){
  Json result=*mission;
  result["retain"]=false;
  result.erase("messages");
  result["diskLoaded"]=false;
  const auto& status=value::property(*mission,"status");
  if(status.is_string()&&isTerminalMissionStatus(status.get<std::string>())){
    if(!services.nowMillis)
      throw std::invalid_argument("Task release clock import required");
    result["evictAfter"]=services.nowMillis()+panelGraceMs;
  }else result.erase("evictAfter");
  return std::make_shared<Json>(std::move(result));
}
void enterPartnerView(const std::string& missionId,
  const SetAppState& setAppState,const PartnerViewServices& services){
  if(!setAppState)throw std::invalid_argument("AppState setter required");
  setAppState([missionId,services](const AppStateRef& previous){
    const auto mission=lookup(previous,missionId);
    const auto prevId=viewedId(previous);
    const auto prevMission=prevId?lookup(previous,*prevId):value::ValueRef{};
    const bool switching=prevId&&*prevId!=missionId&&
      isLocalBat(prevMission)&&retained(prevMission);
    const bool needsRetain=isLocalBat(mission)&&
      (!retained(mission)||mission->contains("evictAfter"));
    const bool needsView=!prevId||*prevId!=missionId||
      value::property(previous->snapshot(),"viewSelectionMode")!=
        "viewing-agent";
    if(!needsRetain&&!needsView&&!switching)return previous;
    auto next=std::make_shared<AppState>(previous->shallowCopy());
    if(switching||needsRetain){
      auto missions=std::make_shared<OrderedStringMap<value::ValueRef>>(
        *previous->missions);
      if(switching)missions->set(*prevId,releaseLocalBat(prevMission,services));
      if(needsRetain){
        Json retainedMission=*mission;
        retainedMission["retain"]=true;
        retainedMission.erase("evictAfter");
        missions->set(missionId,std::make_shared<Json>(std::move(retainedMission)));
      }
      replaceMissions(*next,std::move(missions));
    }
    next->fields->set("viewingAgentTaskId",missionId);
    next->fields->set("viewSelectionMode","viewing-agent");
    return next;
  });
}
void exitPartnerView(const SetAppState& setAppState,
  const PartnerViewServices& services){
  if(!setAppState)throw std::invalid_argument("AppState setter required");
  setAppState([services](const AppStateRef& previous){
    const auto id=viewedId(previous);
    auto cleared=std::make_shared<AppState>(previous->shallowCopy());
    cleared->fields->setRef("viewingAgentTaskId",{});
    cleared->fields->set("viewSelectionMode","none");
    if(!id)return value::property(previous->snapshot(),"viewSelectionMode")==
      "none"?previous:cleared;
    const auto mission=lookup(previous,*id);
    if(!isLocalBat(mission)||!retained(mission))return cleared;
    auto missions=std::make_shared<OrderedStringMap<value::ValueRef>>(
      *previous->missions);
    missions->set(*id,releaseLocalBat(mission,services));
    replaceMissions(*cleared,std::move(missions));
    return cleared;
  });
}
void stopOrDismissBat(const std::string& missionId,
  const SetAppState& setAppState,const PartnerViewServices& services){
  if(!setAppState)throw std::invalid_argument("AppState setter required");
  setAppState([missionId,services](const AppStateRef& previous){
    const auto mission=lookup(previous,missionId);
    if(!isLocalBat(mission))return previous;
    if(value::property(*mission,"status")=="running"){
      if(!services.getAbortController)
        throw std::invalid_argument("Task abort-controller import required");
      if(auto abort=services.getAbortController(mission))(*abort)();
      return previous;
    }
    if(value::property(*mission,"evictAfter")==0)return previous;
    const auto id=viewedId(previous);
    const bool viewingThis=id&&*id==missionId;
    auto next=std::make_shared<AppState>(previous->shallowCopy());
    auto missions=std::make_shared<OrderedStringMap<value::ValueRef>>(
      *previous->missions);
    Json released=*releaseLocalBat(mission,services);
    released["evictAfter"]=0;
    missions->set(missionId,std::make_shared<Json>(std::move(released)));
    replaceMissions(*next,std::move(missions));
    if(viewingThis){
      next->fields->setRef("viewingAgentTaskId",{});
      next->fields->set("viewSelectionMode","none");
    }
    return next;
  });
}
} // namespace batbox::app_state
