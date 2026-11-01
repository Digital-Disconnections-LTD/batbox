#include "bats/BatMissionNotification.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
template<class T>void need(const T& callback,const char* key){
  if(!callback)throw std::invalid_argument(std::string("Unbound LocalAgentTask import: ")+key);
}
std::string string(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  return value::stringify(value);
}
std::string tag(const char* name,const std::string& content){
  return std::string("<")+name+">"+content+"</"+name+">";
}
}
void enqueueBatNotification(const Json& args,
  BatMissionNotificationServices& services){
  const Json& idValue=field(args,"taskId");
  if(!idValue.is_string())throw std::invalid_argument("Missing notification taskId");
  const std::string id=idValue.get<std::string>();
  bool shouldEnqueue=false;
  need(services.setAppState,"setAppState");
  services.setAppState([&](const Json& previous){
    const Json& missions=field(previous,"tasks");
    const Json& mission=value::property(missions,id);
    if(mission.is_null())return previous;
    if(value::truthy(field(mission,"notified")))return previous;
    shouldEnqueue=true;
    Json next=previous;
    next["tasks"][id]["notified"]=true;
    return next;
  });
  if(!shouldEnqueue)return;
  need(services.abortSpeculation,"abortSpeculation");
  services.abortSpeculation(services.setAppState);
  const std::string description=string(field(args,"description"));
  const std::string status=string(field(args,"status"));
  const Json& error=field(args,"error");
  const std::string summary=status=="completed"?
    "Agent \""+description+"\" completed":status=="failed"?
    "Agent \""+description+"\" failed: "+
      (value::truthy(error)?string(error):"Unknown error"):
    "Agent \""+description+"\" was stopped";
  need(services.getMissionOutputPath,"getTaskOutputPath");
  const std::string outputPath=services.getMissionOutputPath(id);
  const Json& gadgetUseId=field(args,"toolUseId");
  const std::string gadgetUseIdLine=value::truthy(gadgetUseId)?
    "\n"+tag("tool-use-id",string(gadgetUseId)):"";
  const Json& final=field(args,"finalMessage");
  const std::string resultSection=value::truthy(final)?
    "\n"+tag("result",string(final)):"";
  const Json& usage=field(args,"usage");
  const std::string usageSection=value::truthy(usage)?
    "\n<usage>"+tag("total_tokens",string(field(usage,"totalTokens")))+
      tag("tool_uses",string(field(usage,"toolUses")))+
      tag("duration_ms",string(field(usage,"durationMs")))+"</usage>":"";
  const Json& worktreePath=field(args,"worktreePath");
  const Json& worktreeBranch=field(args,"worktreeBranch");
  const std::string worktreeSection=value::truthy(worktreePath)?
    "\n<worktree>"+tag("worktreePath",string(worktreePath))+
      (value::truthy(worktreeBranch)?tag("worktreeBranch",string(worktreeBranch)):"")+
      "</worktree>":"";
  const std::string message="<task-notification>\n"+
    tag("task-id",id)+gadgetUseIdLine+"\n"+
    tag("output-file",outputPath)+"\n"+
    tag("status",status)+"\n"+
    tag("summary",summary)+resultSection+usageSection+worktreeSection+
    "\n</task-notification>";
  need(services.enqueuePendingNotification,"enqueuePendingNotification");
  services.enqueuePendingNotification(Json{{"value",message},
    {"mode","task-notification"}});
}
std::string batQueuedInputNotification(const std::string& missionId,
    const std::string& label,const std::string& reason){
  if(missionId.empty())throw std::invalid_argument("Missing notification taskId");
  return "<task-notification>\n"+tag("task-id",missionId)+"\n"+
    tag("status","awaiting_resume")+"\n"+
    tag("summary","Bat \""+(label.empty()?missionId:label)+
      "\" has queued input awaiting resume: "+reason)+
    "\n</task-notification>";
}
} // namespace batbox::bats
