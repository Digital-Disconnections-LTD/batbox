#include "application_bats/IndigenousBatApplication.hpp"
#include "application/Async.hpp"
#include "runtime/IndigenousPath.hpp"
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string string(const Json& value){return value.is_string()?value.get<std::string>():"";}
}
std::string IndigenousBatApplication::directInputPath(const std::string& id)const{
  return runtime::pathToUtf8(inputs_.config.stateDirectory/"tasks"/(id+".inputs.jsonl"));
}
Json IndigenousBatApplication::queueDirectInput(const std::string& id,const std::string& text,bool echo){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Direct input journal requires owner");
  Json message=messages::createUserMessage(Json{{"content",text}},*inputs_.messages);
  // This ledger is separate from the transcript chain so an instruction submitted
  // before the initial transcript write cannot become its accidental root.
  // The same canonical UUID enters the chain when drained or resumed.
  (void)writes_->enqueueWrite(directInputPath(id),Json{{"type","direct_input"},{"message",message}});
  pendingDirectInputs_[id].push_back(message);
  if(echo)bats::appendMessageToLocalBat(id,message,missionState_);
  return message;
}
std::vector<Json> IndigenousBatApplication::readDirectInputs(const std::string& id)const{
  std::ifstream input(runtime::pathFromUtf8(directInputPath(id)),std::ios::binary);
  std::vector<Json> output;std::unordered_set<std::string> seen;
  std::string line;
  while(std::getline(input,line)){
    try{
      const auto row=value::parse(line);const auto& message=field(row,"message");
      const auto uuid=string(field(message,"uuid"));
      if(field(row,"type")=="direct_input"&&!uuid.empty()&&seen.insert(uuid).second)
        output.push_back(message);
    }catch(const std::exception&){} // A torn final journal line preserves its prefix.
  }
  return output;
}
void IndigenousBatApplication::schedulePendingContinuation(const std::string& id,bool allowMissing){
  const auto weak=weak_from_this();
  inputs_.owner.postMission([weak,id,allowMissing]{
    const auto self=weak.lock();if(!self||self->closing_)return;
    if(self->pendingContinuationScheduled_.contains(id))return;
    const auto queued=self->pendingDirectInputs_.find(id);
    if(queued==self->pendingDirectInputs_.end()||queued->second.empty())return;
    {
      std::lock_guard lock(self->conversationMutex_);
      if(self->batJobs_.contains(id)||self->pendingResumes_.contains(id))return;
    }
    const auto state=self->appState();const auto mission=value::property(field(state,"tasks"),id);
    const auto status=field(mission,"status");
    if((mission.is_null()&&!allowMissing)||(!mission.is_null()&&status!="completed")||
        !field(mission,"inputResumeError").is_null())return;
    self->pendingContinuationScheduled_.insert(id);
    const auto batch=queued->second;
    const auto text=string(field(field(batch.back(),"message"),"content"));
    auto continuation=self->submitToMissionWithContext(id,text,{}, {},false,batch);
    continuation.then([self,id](const event_loop::Unit*,const std::exception_ptr& error){
      self->pendingContinuationScheduled_.erase(id);
      if(!error)return;
      std::string message="Unable to continue queued agent input";
      try{std::rethrow_exception(error);}catch(const std::exception& failure){message=failure.what();}catch(...){}
      self->setAppState([id,message](const Json& state){
        if(!bats::isPanelBatMission(value::property(field(state,"tasks"),id)))return state;
        Json next=state;next["tasks"][id]["inputResumeError"]=message;return next;
      });
      self->inputs_.enqueuePendingNotification(Json{{"value","Agent "+id+
        " has durable queued input awaiting resume: "+message},{"mode","task-notification"}});
    });
  });
}
} // namespace batbox::application_bats
