#include "missions/shell/Roster.hpp"
#include <algorithm>
#include <chrono>
#include <utility>

namespace batbox::shell_missions {
StopMissionError::StopMissionError(std::string message,std::string errorCode):
  std::runtime_error(std::move(message)),code(std::move(errorCode)){}
std::string escapeXml(const std::string& text){
  std::string result;
  for(char c:text){
    if(c=='&')result+="&amp;";
    else if(c=='<')result+="&lt;";
    else if(c=='>')result+="&gt;";
    else result+=c;
  }
  return result;
}
Roster::Roster(Services services):services_(std::move(services)){
  if(!services_.nowMs||!services_.enqueueNotification||!services_.onMissionTerminated)
    throw std::invalid_argument("task registry clock, notification, and termination services required");
}
void Roster::attachShell(std::shared_ptr<shell::ManagedShell> shell){
  if(!shell)throw std::invalid_argument("managed shell required");
  std::lock_guard lock(mutex_);shell_=std::move(shell);
}
void Roster::completeLocked(MissionRecord& mission,const shell::ExecResult& result,
  std::optional<std::string>& notification){
  if(mission.status=="killed")return;
  mission.status=result.code==0?"completed":"failed";
  mission.exitCode=result.code;mission.interrupted=result.interrupted;mission.endTimeMs=services_.nowMs();
  if(mission.notified)return;
  mission.notified=true;
  const auto status=mission.status;
  const auto summary=status=="completed"?"Background command \""+mission.description+
    "\" completed (exit code "+std::to_string(result.code)+")":
    "Background command \""+mission.description+"\" failed with exit code "+std::to_string(result.code);
  if(!shell_)throw std::logic_error("managed shell not attached");
  notification="<task-notification>\n<task-id>"+mission.id+"</task-id>"+
    (mission.gadgetUseId?"\n<tool-use-id>"+*mission.gadgetUseId+"</tool-use-id>":"")+
    "\n<output-file>"+shell_->outputPath(mission.id)+"</output-file>\n<status>"+
    status+"</status>\n<summary>"+escapeXml(summary)+"</summary>\n</task-notification>";
}
void Roster::registerShell(MissionRecord record){
  if(record.id.empty())throw std::invalid_argument("task ID required");
  record.type="local_bash";record.status="running";
  if(record.startTimeMs==0)record.startTimeMs=services_.nowMs();
  std::optional<std::string> notification,bat;
  {
    std::lock_guard lock(mutex_);
    if(!shell_)throw std::logic_error("managed shell not attached");
    auto [it,inserted]=missions_.emplace(record.id,std::move(record));
    if(!inserted)throw std::invalid_argument("task already registered: "+it->first);
    bat=it->second.batId;
    auto early=earlyCompletions_.find(it->first);
    if(early!=earlyCompletions_.end()){
      completeLocked(it->second,early->second,notification);earlyCompletions_.erase(early);
    }
  }
  cv_.notify_all();
  if(notification)services_.enqueueNotification(*notification,bat);
}
void Roster::registerForeground(MissionRecord record){
  record.isBackgrounded=false;
  registerShell(std::move(record));
}
void Roster::adoptBackground(MissionRecord record){
  if(record.id.empty())throw std::invalid_argument("task ID required");
  record.type="local_bash";record.isBackgrounded=true;
  if(record.startTimeMs==0)record.startTimeMs=services_.nowMs();
  std::optional<std::string> notification,bat;
  {
    std::lock_guard lock(mutex_);
    if(!shell_)throw std::logic_error("managed shell not attached");
    const auto it=missions_.find(record.id);
    if(it!=missions_.end()){
      if(it->second.type!="local_bash")throw std::invalid_argument("task ID belongs to another type");
      it->second.isBackgrounded=true;
      return;
    }
    record.status="running";
    auto inserted=missions_.emplace(record.id,std::move(record)).first;
    bat=inserted->second.batId;
    auto early=earlyCompletions_.find(inserted->first);
    if(early!=earlyCompletions_.end()){
      completeLocked(inserted->second,early->second,notification);earlyCompletions_.erase(early);
    }
  }
  cv_.notify_all();
  if(notification)services_.enqueueNotification(*notification,bat);
}
bool Roster::backgroundExistingForeground(const std::string& id){
  std::lock_guard lock(mutex_);
  const auto it=missions_.find(id);
  if(it==missions_.end()||it->second.type!="local_bash"||it->second.isBackgrounded||
     it->second.status!="running")return false;
  if(!shell_)throw std::logic_error("managed shell not attached");
  if(!shell_->background(id,true))return false;
  it->second.isBackgrounded=true;
  cv_.notify_all();
  return true;
}
bool Roster::unregisterForeground(const std::string& id){
  std::lock_guard lock(mutex_);
  const auto it=missions_.find(id);
  if(it==missions_.end()||it->second.type!="local_bash"||it->second.isBackgrounded)return false;
  missions_.erase(it);cv_.notify_all();return true;
}
std::vector<std::string> Roster::foregroundShellIds()const{
  std::vector<std::string> ids;
  std::lock_guard lock(mutex_);
  for(const auto& [id,mission]:missions_)if(mission.type=="local_bash"&&mission.status=="running"&&
    !mission.isBackgrounded)ids.push_back(id);
  return ids;
}
std::vector<std::string> Roster::backgroundAllForeground(){
  std::vector<std::string> backgrounded;
  for(const auto& id:foregroundShellIds())if(backgroundExistingForeground(id))backgrounded.push_back(id);
  return backgrounded;
}
void Roster::registerExternal(MissionRecord record){
  if(record.id.empty())throw std::invalid_argument("task ID required");
  if(record.type=="local_bash")throw std::invalid_argument("external task must have external type");
  std::lock_guard lock(mutex_);
  if(!missions_.emplace(record.id,std::move(record)).second)
    throw std::invalid_argument("task already registered");
  cv_.notify_all();
}
void Roster::upsertExternal(MissionRecord record){
  if(record.id.empty()||record.type=="local_bash")
    throw std::invalid_argument("external task ID and type required");
  {
    std::lock_guard lock(mutex_);
    const auto it=missions_.find(record.id);
    if(it!=missions_.end()&&it->second.type=="local_bash")
      throw std::invalid_argument("task ID belongs to a shell task");
    missions_.insert_or_assign(record.id,std::move(record));
  }
  cv_.notify_all();
}
void Roster::eraseExternal(const std::string& id){
  {
    std::lock_guard lock(mutex_);
    const auto it=missions_.find(id);
    if(it!=missions_.end()&&it->second.type!="local_bash")missions_.erase(it);
  }
  cv_.notify_all();
}
void Roster::bindExternalServices(
    std::function<void(const MissionRecord&)> stop,
    std::function<std::string(const MissionRecord&)> read,
    std::function<void(const MissionRecord&)> markNotified){
  std::lock_guard lock(mutex_);
  services_.stopExternalMission=std::move(stop);
  services_.readExternalOutput=std::move(read);
  services_.markExternalNotified=std::move(markNotified);
}
void Roster::completeShell(const std::string& missionId,const shell::ExecResult& result){
  std::optional<std::string> notification,bat;
  {
    std::lock_guard lock(mutex_);
    auto it=missions_.find(missionId);
    if(it==missions_.end()){earlyCompletions_[missionId]=result;return;}
    bat=it->second.batId;
    completeLocked(it->second,result,notification);
  }
  cv_.notify_all();
  if(notification)services_.enqueueNotification(*notification,bat);
}
std::optional<MissionRecord> Roster::get(const std::string& id)const{
  std::lock_guard lock(mutex_);auto it=missions_.find(id);
  if(it==missions_.end())return std::nullopt;
  return it->second;
}
std::vector<MissionRecord> Roster::list()const{
  std::lock_guard lock(mutex_);std::vector<MissionRecord> result;
  result.reserve(missions_.size());for(const auto& [_,mission]:missions_)result.push_back(mission);
  return result;
}
std::optional<MissionRecord> Roster::wait(const std::string& id,double timeoutMs,std::stop_token signal){
  std::stop_callback stopWake(signal,[this]{cv_.notify_all();});
  const auto start=std::chrono::steady_clock::now();
  for(;;){
    const auto elapsed=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-start).count();
    if(elapsed>=timeoutMs){std::lock_guard lock(mutex_);
      auto it=missions_.find(id);return it==missions_.end()?std::nullopt:std::optional(it->second);}
    if(signal.stop_requested())throw MissionAbortError();
    std::unique_lock lock(mutex_);
    auto it=missions_.find(id);if(it==missions_.end())return std::nullopt;
    if(it->second.status!="running"&&it->second.status!="pending")return it->second;
    const auto slice=std::min(100.0,timeoutMs-elapsed);
    cv_.wait_for(lock,std::chrono::duration<double,std::milli>(slice));
  }
}
bool Roster::markNotified(const std::string& id){
  MissionRecord mission;
  {
    std::lock_guard lock(mutex_);auto it=missions_.find(id);
    if(it==missions_.end()||it->second.notified)return false;
    it->second.notified=true;mission=it->second;
  }
  if(mission.type!="local_bash"&&services_.markExternalNotified)
    services_.markExternalNotified(mission);
  return true;
}
StopResult Roster::stop(const std::string& id){
  MissionRecord mission;bool local=false;
  {
    std::lock_guard lock(mutex_);
    auto it=missions_.find(id);
    if(it==missions_.end())throw StopMissionError("No task found with ID: "+id,"not_found");
    if(it->second.status!="running")throw StopMissionError(
      "Task "+id+" is not running (status: "+it->second.status+")","not_running");
    mission=it->second;local=mission.type=="local_bash";
    if(!local&&!services_.stopExternalMission)
      throw StopMissionError("Unsupported task type: "+mission.type,"unsupported_type");
    if(local){it->second.status="killed";it->second.notified=true;
      it->second.endTimeMs=services_.nowMs();}
  }
  if(local){
    if(!shell_)throw std::logic_error("managed shell not attached");
    (void)shell_->stop(id);
    services_.onMissionTerminated(mission);
  }else{
    services_.stopExternalMission(mission);
    std::lock_guard lock(mutex_);auto it=missions_.find(id);
    if(it!=missions_.end()){it->second.status="killed";it->second.endTimeMs=services_.nowMs();}
  }
  cv_.notify_all();
  return {id,mission.type,local?std::optional(mission.command):std::optional(mission.description)};
}
void Roster::stopForBat(const std::string& batId){
  std::vector<std::string> ids;
  {
    std::lock_guard lock(mutex_);
    for(const auto& [id,mission]:missions_)if(mission.type=="local_bash"&&mission.batId==batId&&mission.status=="running")
      ids.push_back(id);
  }
  for(const auto& id:ids)try{(void)stop(id);}catch(const StopMissionError&){}
}
std::string Roster::readOutput(const MissionRecord& mission)const{
  if(mission.type=="local_bash"){
    if(!shell_)throw std::logic_error("managed shell not attached");
    return shell_->readOutput(mission.id);
  }
  if(!services_.readExternalOutput)throw std::logic_error("external task output reader not supplied");
  return services_.readExternalOutput(mission);
}
} // namespace batbox::shell_missions
