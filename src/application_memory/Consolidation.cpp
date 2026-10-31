#include "application_memory/Provider.hpp"
#include "application_memory/Consolidation.hpp"
#include "application_memory/IndigenousWork.hpp"
#include "application/Async.hpp"
#include "messages/RuntimeMessages.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include "utils/envUtils.hpp"
#include <cmath>
#include <filesystem>
#include <set>

namespace batbox::application_memory {
namespace {
using Unit=event_loop::Unit;
double positiveSetting(const Json& settings,const char* key,double fallback){
  const auto& value=value::property(value::property(settings,"autoDream"),key);
  return value.is_number()&&std::isfinite(value.get<double>())&&value.get<double>()>0?
    value.get<double>():fallback;
}
std::string lastUuid(const Json& context){
  const auto& messages=value::property(context,"messages");
  if(!messages.is_array())return {};
  for(auto message=messages.rbegin();message!=messages.rend();++message){
    const auto& id=value::property(*message,"uuid");
    if(id.is_string())return id.get<std::string>();
  }
  return {};
}
struct Decision {bool run=false,scanned=false;double prior=0;std::vector<std::string> flights;std::string result;};
}
void Provider::updateMission(const std::string& id,const std::function<Json(Json)>& update){
  requireOwner();
  auto set=inputs_.updateAppState;
  if(!set&&inputs_.activeBatContext){
    if(auto bat=inputs_.activeBatContext())
      set=bat->setAppStateForMissions?bat->setAppStateForMissions:bat->setAppState;
  }
  if(!set)return;
  set([id,update](const Json& previous){
    auto next=previous;
    auto& missions=next["tasks"];if(!missions.is_object())missions=Json::object();
    missions[id]=update(value::property(missions,id));return next;
  });
}
void Provider::executeAutoDream(const Json& context){
  requireOwner();
  if(closing_||activeDream_)return;
  const auto id=lastUuid(context);
  if(!id.empty()&&id==lastTurnUuid_)return;
  lastTurnUuid_=id;
  auto pathServices=paths();
  if(!instructions::isAutoMemoryEnabled(pathServices)||!dreamEnabled()||
      isEnvTruthy(value::property(pathServices.environment,pathServices.keys.remoteMode)))return;
  launchDream(context,false);
}
event_loop::Deferred<Unit> Provider::consolidate(const Json& context){
  requireOwner();return launchDream(context,true);
}
event_loop::Deferred<Unit> Provider::launchDream(const Json& context,bool manual){
  requireOwner();
  if(activeDream_)return *activeDream_;
  event_loop::Deferred<Unit> output(inputs_.owner);
  if(closing_){output.resolve({});return output;}
  if(!instructions::isAutoMemoryEnabled(paths())){
    lastResult_="Auto memory is disabled";output.resolve({});return output;
  }
  std::shared_ptr<bats::StashSafeParams> stash;
  if(context.is_object()&&value::property(context,"messages").is_array()&&
      inputs_.activeBatContext){
    if(auto active=inputs_.activeBatContext()){
      auto query=active->query;
      const auto& live=value::property(context,"toolUseContext");
      if(live.is_object())query.data=live;
      stash=bats::makeStashSafeSnapshot(active,value::property(context,"systemPrompt"),
        value::property(context,"userContext"),value::property(context,"systemContext"),
        query,context.at("messages"));
    }
  }
  if(!stash)stash=bats::getLastStashSafeParams();
  if(!stash||!inputs_.currentEngine){
    lastResult_="A completed conversation turn is needed before consolidation";
    if(manual)output.reject(std::make_exception_ptr(std::runtime_error(lastResult_)));
    else output.resolve({});
    return output;
  }
  const auto directory=autoMemoryDirectory();
  const auto transcript=runtime::pathToUtf8(runtime::pathFromUtf8(
    inputs_.flight->paths()->getTranscriptPath()).parent_path());
  const auto now=static_cast<double>(platform::nowMillis());
  const auto minHours=positiveSetting(settings(),"minHours",24);
  const auto minFlights=positiveSetting(settings(),"minSessions",5);
  const auto lastScan=lastFlightScanAt_;
  const auto flight=inputs_.flight->id();
  activeDream_=output;dreamStop_=std::make_shared<std::stop_source>();
  const auto stop=dreamStop_->get_token();
  auto self=shared_from_this();
  memoryIO<Decision>(inputs_.owner,[directory,transcript,now,lastScan,minHours,minFlights,flight,manual]{
    Decision decision;
    const auto last=readLastConsolidatedAt(directory);
    if(!manual&&(now-last)/3600000<minHours){decision.result="Waiting for consolidation interval";return decision;}
    if(!manual&&now-lastScan<10*60*1000){decision.result="Waiting for session scan interval";return decision;}
    decision.scanned=true;
    decision.flights=flightsTouchedSince(transcript,last,flight);
    if(!manual&&decision.flights.size()<minFlights){decision.result="Waiting for more completed sessions";return decision;}
    const auto prior=tryAcquireConsolidationLock(directory,now);
    if(!prior){decision.result="Another session holds the consolidation lock";return decision;}
    decision.run=true;decision.prior=*prior;return decision;
  }).then([self,output,stash,directory,transcript,now,stop,manual](
      const Decision* decision,const std::exception_ptr& error){
    if(error||!decision||!decision->run){
      if(error||(decision&&decision->scanned))self->lastFlightScanAt_=now;
      if(decision)self->lastResult_=decision->result;
      if(error){try{std::rethrow_exception(error);}catch(const std::exception& failure){self->lastResult_=failure.what();}}
      self->activeDream_.reset();self->dreamStop_.reset();
      if(error&&manual)output.reject(error);else output.resolve({});return;
    }
    self->lastFlightScanAt_=now;self->lastResult_="Consolidation running";
    const auto missionId="dream-"+self->inputs_.messages->randomUUID();
    const auto flightCount=decision->flights.size();
    const auto acquiredPrior=decision->prior;
    self->updateMission(missionId,[missionId,now,flightCount,acquiredPrior](Json){return Json{{"id",missionId},{"type","dream"},
      {"status","running"},{"phase","starting"},{"description","Memory consolidation"},
      {"startTime",now},{"sessionsReviewing",flightCount},
      {"filesTouched",Json::array()},{"turns",Json::array()},
      {"isBackgrounded",true},{"priorMtime",acquiredPrior}};});
    const auto prior=decision->prior;
    std::string extra="\n\n**Tool constraints for this run:** Bash is restricted to read-only commands (`ls`, `find`, `grep`, `cat`, `stat`, `wc`, `head`, `tail`, and similar). Anything that writes, redirects to a file, or modifies state will be denied. Plan your exploration with this in mind — no need to probe.\n\nSessions since last consolidation ("+
      std::to_string(decision->flights.size())+"):\n";
    for(std::size_t i=0;i<decision->flights.size();++i){
      if(i)extra+='\n';
      extra+="- "+decision->flights[i];
    }
    const auto poetry=buildConsolidationPoetry(directory,transcript,extra);
    auto progress=[self,missionId](const Json& message){
      std::string text;int count=0;std::vector<std::string> touched;
      const auto& content=value::property(value::property(message,"message"),"content");
      if(content.is_array())for(const auto& block:content){
        if(value::property(block,"type")=="text"&&value::property(block,"text").is_string())text+=block.at("text").get<std::string>();
        if(value::property(block,"type")!="tool_use")continue;
        ++count;const auto& name=value::property(block,"name");
        if(name=="Edit"||name=="Write"){
          const auto& path=value::property(value::property(block,"input"),"file_path");
          if(path.is_string())touched.push_back(path.get<std::string>());
        }
      }
      if(value::trim(text).empty()&&count==0&&touched.empty())return;
      self->updateMission(missionId,[text=value::trim(text),count,touched](Json mission){
        auto& files=mission["filesTouched"];if(!files.is_array())files=Json::array();
        for(const auto& path:touched){
          bool found=false;for(const auto& old:files)if(old==path){found=true;break;}
          if(!found){files.push_back(path);mission["phase"]="updating";}
        }
        auto& turns=mission["turns"];if(!turns.is_array())turns=Json::array();
        if(turns.size()>=30)turns.erase(turns.begin());
        turns.push_back(Json{{"text",text},{"toolUseCount",count}});return mission;
      });
    };
    application::runInWorker<Json>(self->inputs_.owner,[self,stash,poetry,stop,progress]{
      return self->runFork(stash,poetry,"auto_dream",stop,progress);
    }).then([self,output,directory,prior,missionId,manual](const Json*,const std::exception_ptr& failure){
      const bool stopped=self->dreamStop_&&self->dreamStop_->stop_requested();
      auto finish=[self,output,missionId,failure,stopped,manual](const Unit*,const std::exception_ptr& rollbackError){
        if(rollbackError)self->inputs_.log("Memory consolidation lock rollback failed");
        self->lastResult_=stopped?"Consolidation canceled":failure?"Consolidation failed":"Consolidation completed";
        self->updateMission(missionId,[failure,stopped](Json mission){
          mission["status"]=stopped?"killed":failure?"failed":"completed";
          mission["notified"]=true;mission["endTime"]=platform::nowMillis();return mission;});
        if(!failure)self->refresh();
        self->activeDream_.reset();self->dreamStop_.reset();
        if(self->inputs_.notify)self->inputs_.notify(Json{{"type","memory_maintenance"},
          {"message",self->lastResult_},{"taskId",missionId}});
        if(failure&&manual)output.reject(failure);else output.resolve({});
      };
      if(failure)memoryIO<Unit>(self->inputs_.owner,[directory,prior]{
        rollbackConsolidationLock(directory,prior);return Unit{};
      }).then(finish);
      else finish(nullptr,{});
    });
  });
  return output;
}
event_loop::Deferred<Unit> Provider::shutdown(){
  requireOwner();closing_=true;
  if(dreamStop_)dreamStop_->request_stop();
  for(auto& stop:recallStops_)stop.request_stop();
  event_loop::Deferred<Unit> ready(inputs_.owner);
  if(!activeDream_&&recallJobs_==0){ready.resolve({});return ready;}
  auto self=shared_from_this();
  const auto afterDream=[self,ready](const Unit*,const std::exception_ptr&){
    if(self->recallJobs_==0)ready.resolve({});
    else self->idleWaiters_.push_back(ready);
  };
  if(activeDream_)activeDream_->then(afterDream);else afterDream(nullptr,{});
  return ready;
}
} // namespace batbox::application_memory
