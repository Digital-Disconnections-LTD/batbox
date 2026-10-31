#include "query/StopHooksInternal.hpp"

namespace batbox::query::stop_internal {
namespace {
Json done(const Json& errors=Json::array(),bool prevented=false){
  return Json{{"blockingErrors",errors},{"preventContinuation",prevented}};
}
struct PartnerState {
  Json blockingErrors=Json::array();
  std::string gadgetUseID;
  bool prevented=false;
  bool aborted=false;
};
bool consume(const Json& result,const std::string& hookName,
  const execution::Context& context,PartnerState& state,
  const std::function<Json(const Json&)>& getHookMessage,
  StopHookServices& services,const Yield& yield){
  const Json& message=field(result,"message");
  if(value::truthy(message)){
    if(field(message,"type")=="progress"&&value::truthy(field(message,"toolUseID")))
      state.gadgetUseID=stringField(message,"toolUseID");
    yield(message);
  }
  const Json& blocking=field(result,"blockingError");
  if(value::truthy(blocking)){
    need(getHookMessage,"getHookMessage");need(services.createUserMessage,"createUserMessage");
    const Json user=services.createUserMessage(Json{{"content",getHookMessage(blocking)},
      {"isMeta",true}});
    state.blockingErrors.push_back(user);
    yield(user);
  }
  if(value::truthy(field(result,"preventContinuation"))){
    state.prevented=true;
    std::string reason=stringField(result,"stopReason");
    if(reason.empty())reason=hookName+" hook prevented continuation";
    need(services.createAttachmentMessage,"createAttachmentMessage");
    yield(services.createAttachmentMessage(Json{{"type","hook_stopped_continuation"},
      {"message",reason},{"hookName",hookName},{"toolUseID",state.gadgetUseID},
      {"hookEvent",hookName}}));
  }
  if(context.stop.stop_requested()){state.aborted=true;return false;}
  return true;
}
}

Json runPartnerHooks(const Json& permissionMode,const execution::Context& context,
  StopHookServices& services,const Yield& yield){
  need(services.getBatName,"getAgentName");need(services.getTeamName,"getTeamName");
  const std::string partner=services.getBatName().value_or("");
  const std::string team=services.getTeamName().value_or("");
  PartnerState state;
  need(services.getMissionListId,"getTaskListId");need(services.listMissions,"listTasks");
  const Json missions=services.listMissions(services.getMissionListId());
  if(missions.is_array())for(const Json& mission:missions){
    if(field(mission,"status")!="in_progress"||field(mission,"owner")!=partner)continue;
    need(services.executeMissionCompletedHooks,"executeTaskCompletedHooks");
    services.executeMissionCompletedHooks(Json{{"id",field(mission,"id")},
      {"subject",field(mission,"subject")},{"description",field(mission,"description")},
      {"teammateName",partner},{"teamName",team},{"permissionMode",permissionMode},
      {"toolUseContext",context.data}},
      [&](const Json& result){return consume(result,"TaskCompleted",context,state,
        services.getMissionCompletedHookMessage,services,yield);});
    if(state.aborted)return done(Json::array(),true);
  }
  need(services.executePartnerIdleHooks,"executeTeammateIdleHooks");
  services.executePartnerIdleHooks(Json{{"teammateName",partner},{"teamName",team},
    {"permissionMode",permissionMode}},
    [&](const Json& result){return consume(result,"TeammateIdle",context,state,
      services.getPartnerIdleHookMessage,services,yield);});
  if(state.aborted||state.prevented)return done(Json::array(),true);
  if(!state.blockingErrors.empty())return done(state.blockingErrors,false);
  return done();
}
} // namespace batbox::query::stop_internal
