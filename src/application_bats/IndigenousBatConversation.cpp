#include "application_bats/IndigenousBatApplication.hpp"
#include "application/Async.hpp"
#include "bats/ResumeFilters.hpp"
#include "interactive/StreamReducer.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include "flights/Load.hpp"
#include "flights/scanning/Scanning.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <regex>
#include <stdexcept>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string string(const Json& value){return value.is_string()?value.get<std::string>():"";}
double timestamp(const std::string& value){
  int y=0,m=0,d=0,h=0,n=0,s=0;
  if(std::sscanf(value.c_str(),"%d-%d-%dT%d:%d:%d",&y,&m,&d,&h,&n,&s)!=6)return 0;
  using namespace std::chrono;
  const year_month_day date{year(y),month(static_cast<unsigned>(m)),day(static_cast<unsigned>(d))};
  if(!date.ok())return 0;
  double result=static_cast<double>(duration_cast<milliseconds>(sys_days(date).time_since_epoch()).count());
  result+=static_cast<double>((h*3600+n*60+s)*1000);
  if(const auto dot=value.find('.',19);dot!=std::string::npos){
    int fraction=0,multiplier=100;
    for(auto i=dot+1;i<value.size()&&multiplier>0;++i){
      if(value[i]<'0'||value[i]>'9')break;
      fraction+=(value[i]-'0')*multiplier;multiplier/=10;
    }
    result+=fraction;
  }
  return result;
}
}
std::optional<std::string> IndigenousBatApplication::resolveMissionId(const std::string& name)const{
  const auto state=appState();
  const auto& mission=value::property(field(state,"tasks"),name);
  if(bats::isPanelBatMission(mission))return name;
  const auto& roster=field(state,"agentNameRegistry");
  if(roster.is_array())for(const auto& entry:roster)
    if(entry.is_array()&&entry.size()>1&&entry[0]==name&&entry[1].is_string())
      return entry[1].get<std::string>();
  static const std::regex id("([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}|a[0-9a-fA-F]{6,32})");
  if(std::regex_match(name,id))return name;
  return {};
}
std::optional<Json> IndigenousBatApplication::readTranscript(const std::string& id){
  if(inputs_.owner.isOwnerThread())throw std::logic_error("Agent transcript reads require a worker");
  writes_->flush();
  auto paths=pathsForBat(id);
  flights::LoadServices load;
  flights::bindIndigenousFileReads(load);
  flights::scanning::bindIndigenousLoadScanners(load);
  flights::GraphServices graph;
  graph.parseTimestamp=timestamp;
  graph.localeCompare=[](const std::string& a,const std::string& b){return a.compare(b);};
  graph.logError=[log=inputs_.logError](const std::string& value){if(log)log(std::runtime_error(value));};
  std::lock_guard lock(pathsMutex_);
  auto transcript=flights::getBatTranscript(id,*paths,load,graph);
  if(transcript){
    std::lock_guard conversationLock(conversationMutex_);
    const auto& messages=field(*transcript,"messages");
    if(messages.is_array())for(const auto& message:messages){
      if(field(message,"uuid").is_string())writtenMessageIds_[id].insert(field(message,"uuid").get<std::string>());
    }
    if(messages.is_array()&&!messages.empty()&&!lastTranscriptUuids_.contains(id)&&
        field(messages.back(),"uuid").is_string())
      lastTranscriptUuids_[id]=field(messages.back(),"uuid").get<std::string>();
  }
  // Attachment recording omits queued directives. Indigenous direct input
  // journaling restores missing canonical user entries as the waiting tail;
  // entries already linked into the chain are never duplicated or reordered.
  if(transcript){
    Json messages=field(*transcript,"messages");
    std::set<std::string> existing;
    for(const auto& message:messages)if(field(message,"uuid").is_string())
      existing.insert(field(message,"uuid").get<std::string>());
    for(const auto& message:readDirectInputs(id))
      if(existing.insert(string(field(message,"uuid"))).second)messages.push_back(message);
    (*transcript)["messages"]=std::move(messages);
  }
  return transcript;
}
event_loop::Deferred<Json> IndigenousBatApplication::observeMission(const std::string& value){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent observation requires owner");
  event_loop::Deferred<Json> output(inputs_.owner);
  try{
    if(closing_)throw std::runtime_error("Agent application is shutting down");
    const auto resolved=resolveMissionId(value);
    if(!resolved)throw std::runtime_error("No agent found: "+value);
    const auto id=*resolved;
    const auto state=appState();const auto& mission=value::property(field(state,"tasks"),id);
    if(!bats::isPanelBatMission(mission))throw std::runtime_error("No agent task found: "+id);
    if(field(mission,"retain")==true&&field(mission,"diskLoaded")==true){output.resolve(mission);return output;}
    setAppState([this,id](const Json& state){
      Json next=state;auto& mission=next["tasks"][id];
      mission["retain"]=true;mission.erase("evictAfter");
      if(field(mission,"status")=="running")if(const auto at=latestStreams_.find(id);at!=latestStreams_.end())
        for(auto it=at->second.begin();it!=at->second.end();++it)mission[it.key()]=it.value();
      return next;
    });
    auto self=shared_from_this();++conversationJobs_;
    application::runInWorker<std::optional<Json>>(inputs_.owner,[self,id]{return self->readTranscript(id);}).then(
      [self,id,output](const std::optional<Json>* transcript,const std::exception_ptr& error){
        --self->conversationJobs_;self->checkShutdown();
        if(error){output.reject(error);return;}
        try{
          self->setAppState([&](const Json& state){
            const auto& mission=value::property(field(state,"tasks"),id);
            if(!bats::isPanelBatMission(mission)||field(mission,"retain")!=true||field(mission,"diskLoaded")==true)return state;
            Json next=state;
            // Disk fills the prefix; a UUID already present in live state wins.
            next["tasks"][id]["messages"]=bats::mergeBatTranscriptMessages(
              transcript&&*transcript?field(**transcript,"messages"):Json::array(),field(mission,"messages"));
            next["tasks"][id]["diskLoaded"]=true;return next;
          });
          const auto state=self->appState();
          output.resolve(value::property(field(state,"tasks"),id));
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void IndigenousBatApplication::releaseMission(const std::string& value){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent release requires owner");
  const auto id=resolveMissionId(value);if(!id)return;
  setAppState([&](const Json& state){
    const auto& mission=value::property(field(state,"tasks"),*id);
    if(!bats::isPanelBatMission(mission)||field(mission,"retain")!=true)return state;
    Json next=state;auto& released=next["tasks"][*id];
    released["retain"]=false;released.erase("messages");released["diskLoaded"]=false;
    for(const auto* key:{"streamingText","streamingThinking","streamingToolUses","streamMode"})released.erase(key);
    const auto status=field(mission,"status");
    if(status=="completed"||status=="failed"||status=="killed")
      released["evictAfter"]=platform::nowMillis()+missionState_.panelGraceMs;
    return next;
  });
  releaseInactiveState();
}
void IndigenousBatApplication::dismissMission(const std::string& value){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent dismissal requires owner");
  const auto id=resolveMissionId(value);if(!id)return;
  const auto state=appState();const auto& mission=value::property(field(state,"tasks"),*id);
  if(field(mission,"status")=="running"){stopMission(*id);return;}
  releaseMission(*id);
  setAppState([&](const Json& state){
    if(!bats::isPanelBatMission(value::property(field(state,"tasks"),*id)))return state;
    Json next=state;next["tasks"][*id]["evictAfter"]=0;return next;
  });
}
Json IndigenousBatApplication::drainPendingMessages(const std::string& id){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent message drain requires owner");
  const auto drained=bats::drainPendingMessages(id,missionState_);
  auto& queued=pendingDirectInputs_[id];
  for(const auto& text:drained){
    auto at=std::find_if(queued.begin(),queued.end(),[&](const Json& input){
      return field(field(input,"message"),"content")==text;
    });
    if(at==queued.end())continue;
    const Json canonical=*at;queued.erase(at);
    recordBatMessages(Json::array({canonical}),id,{},false);
  }
  if(queued.empty())pendingDirectInputs_.erase(id);
  return drained;
}
void IndigenousBatApplication::rememberConversation(const std::string& id,
    const bats::BatContext& context,const IndigenousQueryBridge::CanUseGadget& canUse){
  std::lock_guard lock(conversationMutex_);
  conversations_[id]=Conversation{context,canUse};
}
bool IndigenousBatApplication::hasActiveJobs()const{
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent job snapshot requires owner");
  {std::lock_guard lock(capacityMutex_);
    if(backgroundJobs_||conversationJobs_||active_||outstanding_)return true;}
  for(const auto& [_,inputs]:pendingDirectInputs_)if(!inputs.empty())return true;
  // A retained observer is also a live consumer of this provider generation.
  const auto state=appState();const auto& missions=field(state,"tasks");
  if(missions.is_object())for(auto it=missions.begin();it!=missions.end();++it)
    if(field(it.value(),"retain")==true&&ownsMission(it.key()))return true;
  return false;
}
bool IndigenousBatApplication::ownsMission(const std::string& id)const{
  std::lock_guard lock(pathsMutex_);return batFlights_.contains(id);
}
void IndigenousBatApplication::startBatJob(const std::string& id){
  std::lock_guard lock(conversationMutex_);++batJobs_[id];
}
void IndigenousBatApplication::finishBatJob(const std::string& id){
  bool finished=false;
  {std::lock_guard lock(conversationMutex_);
    if(const auto at=batJobs_.find(id);at!=batJobs_.end()){
      if(!--at->second){batJobs_.erase(at);finished=true;}
    }}
  conversationChanged_.notify_all();
  if(finished){
    schedulePendingContinuation(id);
    const auto weak=weak_from_this();
    inputs_.owner.postMission([weak]{
      if(auto self=weak.lock())self->releaseInactiveState();
    });
  }
}
void IndigenousBatApplication::publishStream(const std::string& id,const Json& message){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent stream projection requires owner");
  setAppState([&](const Json& state){
    const auto& mission=value::property(field(state,"tasks"),id);
    if(!bats::isPanelBatMission(mission))return state;
    if(field(mission,"status")!="running"&&field(message,"type")=="stream_event")return state;
    Json updated=mission;
    if(const auto at=latestStreams_.find(id);at!=latestStreams_.end())
      for(auto it=at->second.begin();it!=at->second.end();++it)updated[it.key()]=it.value();
    interactive::StreamCallbacks callbacks;
    callbacks.onMessage=[&](const Json& value){
      if(field(mission,"retain")==true&&bats::isRecordableBatMessage(value))updated["messages"]=
        bats::mergeBatTranscriptMessages(field(updated,"messages"),Json::array({value}));
    };
    callbacks.onUpdateLength=[](const std::string&){};
    callbacks.onSetStreamMode=[&](const std::string& mode){updated["streamMode"]=mode;};
    callbacks.onStreamingText=[&](interactive::TextUpdate update){
      const auto old=field(updated,"streamingText");
      const auto next=update(old.is_string()?std::optional<std::string>(old.get<std::string>()):std::nullopt);
      updated["streamingText"]=next?Json(*next):Json();
    };
    callbacks.onStreamingThinking=[&](interactive::ThinkingUpdate update){
      std::optional<interactive::StreamingThinking> current;
      const auto old=field(updated,"streamingThinking");
      if(old.is_object()){
        current=interactive::StreamingThinking{string(field(old,"thinking")),field(old,"isStreaming")==true,{}};
        if(field(old,"streamingEndedAt").is_number())current->streamingEndedAt=field(old,"streamingEndedAt").get<double>();
      }
      const auto next=update(current);
      updated["streamingThinking"]=Json();
      if(next){
        updated["streamingThinking"]={{"thinking",next->thinking},{"isStreaming",next->isStreaming}};
        if(next->streamingEndedAt)updated["streamingThinking"]["streamingEndedAt"]=*next->streamingEndedAt;
      }
    };
    callbacks.onStreamingGadgetUses=[&](interactive::GadgetUsesUpdate update){
      std::vector<interactive::StreamingGadgetUse> current;
      const auto old=field(updated,"streamingToolUses");
      if(old.is_array())for(const auto& item:old)
        current.push_back({field(item,"index"),field(item,"contentBlock"),string(field(item,"unparsedToolInput"))});
      updated["streamingToolUses"]=Json::array();
      for(const auto& item:update(current))updated["streamingToolUses"].push_back(
        Json{{"index",item.index},{"contentBlock",item.contentBlock},{"unparsedToolInput",item.unparsedGadgetInput}});
    };
    interactive::StreamImports imports;
    imports.connectorTextEnabled=[]{return false;};
    imports.isConnectorTextBlock=[](const Json&){return false;};
    imports.nowMs=[]{return static_cast<double>(platform::nowMillis());};
    interactive::handleMessageFromStream(message,callbacks,imports);
    const auto& event=field(message,"event");
    if(field(message,"type")=="stream_event"&&field(event,"type")=="content_block_start"&&
        field(field(event,"content_block"),"type")=="thinking")
      updated["streamingThinking"]={{"thinking",""},{"isStreaming",true}};
    if(field(message,"type")=="stream_event"&&field(field(event,"delta"),"type")=="thinking_delta"){
      auto text=string(field(field(updated,"streamingThinking"),"thinking"));
      text+=string(field(field(event,"delta"),"thinking"));
      updated["streamingThinking"]={{"thinking",text},{"isStreaming",true}};
    }
    Json projected=Json::object();
    for(const auto* key:{"streamingText","streamingThinking","streamingToolUses","streamMode"})
      if(updated.contains(key))projected[key]=updated[key];
    latestStreams_[id]=std::move(projected);
    if(field(mission,"retain")!=true||updated==mission)return state;
    Json next=state;next["tasks"][id]=std::move(updated);return next;
  });
}
} // namespace batbox::application_bats
