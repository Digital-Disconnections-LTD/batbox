#include "bat_context/PartnerMissions.hpp"
#include <stdexcept>

namespace batbox::bat_context {
bool hasActiveInProcessPartners(const PartnerMissionState& state){
  for(const auto& [id,mission]:state.missions){
    (void)id;
    if(mission&&mission->type=="in_process_teammate"&&mission->status=="running")
      return true;
  }
  return false;
}
bool hasWorkingInProcessPartners(const PartnerMissionState& state){
  for(const auto& [id,mission]:state.missions){
    (void)id;
    if(mission&&mission->type=="in_process_teammate"&&mission->status=="running"&&
       !value::truthy(mission->isIdle))return true;
  }
  return false;
}
event_loop::Deferred<event_loop::Unit> waitForPartnersToBecomeIdle(
    event_loop::Loop& loop,const SetPartnerMissionState& setAppState,
    const PartnerMissionState& appState){
  std::vector<std::string> workingMissionIds;
  for(const auto& [id,mission]:appState.missions){
    if(mission&&mission->type=="in_process_teammate"&&mission->status=="running"&&
       !value::truthy(mission->isIdle))workingMissionIds.push_back(id);
  }
  event_loop::Deferred<event_loop::Unit> output(loop);
  if(workingMissionIds.empty()){
    output.resolve(event_loop::Unit{});
    return output;
  }
  if(!setAppState){
    output.reject(std::make_exception_ptr(std::invalid_argument(
      "Teammate setAppState import required")));
    return output;
  }
  auto remaining=std::make_shared<std::size_t>(workingMissionIds.size());
  auto onIdle=[remaining,output]{
    --*remaining;
    if(*remaining==0)output.resolve(event_loop::Unit{});
  };
  try{setAppState([ids=std::move(workingMissionIds),onIdle](
      const PartnerMissionState& previous){
    PartnerMissionState result=previous;
    for(const auto& id:ids){
      for(auto& [missionId,mission]:result.missions){
        if(missionId!=id)continue;
        if(mission&&mission->type=="in_process_teammate"){
          if(value::truthy(mission->isIdle))onIdle();
          else{
            auto copy=std::make_shared<PartnerMission>(*mission);
            copy->onIdleCallbacks.push_back(onIdle);
            mission=std::move(copy);
          }
        }
        break;
      }
    }
    return result;
  });}
  catch(...){output.reject(std::current_exception());}
  return output;
}
}
