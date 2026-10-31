#include "interactive/background/FlightMission.hpp"
#include "services/tokenEstimation.hpp"
#include <stdexcept>

namespace batbox::interactive::background {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Flight Mission binding required: ")+name);
}
const Json& field(const Json& value,const char* key){return value::property(value,key);}
Json messagesJson(const TurnMessagesRef& messages){
  Json result=Json::array();
  for(const auto& message:*messages)result.push_back(*message);
  return result;
}
}
struct FlightRun:std::enable_shared_from_this<FlightRun> {
  std::shared_ptr<FlightMissions> owner;
  FlightMissionRef mission;
  StartRequest request;
  query::AsyncQueryCursor cursor;
  TurnMessagesRef messages;
  Json activities=Json::array();
  double tokenCount=0;
  std::size_t gadgetCount=0;
  std::optional<std::string> lastUuid;

  FlightRun(std::shared_ptr<FlightMissions> missions,FlightMissionRef current,
      StartRequest input):owner(std::move(missions)),mission(std::move(current)),
        request(std::move(input)),messages(
          std::make_shared<TurnMessages>(*request.messages)){
    if(!messages->empty()){
      const auto& uuid=field(*messages->back(),"uuid");
      if(uuid.is_string())lastUuid=uuid.get<std::string>();
    }
  }
  void record(TurnMessagesRef next,const std::optional<std::string>& previous,
      const std::string& prefix){
    auto& s=*owner->services_;
    need(s.recordSidechainTranscript,"recordSidechainTranscript");
    try{
      const auto pending=s.recordSidechainTranscript(next,mission->id,previous);
      if(!owner->owner_.dispatcher().sameLoop(pending.dispatcher()))
        throw std::invalid_argument("Mission transcript Promise crossed owner loops");
      auto imports=owner->services_;
      pending.then([imports,prefix](const event_loop::Unit*,
          const std::exception_ptr& error){
        if(!error||!imports->logForDebugging)return;
        try{std::rethrow_exception(error);}
        catch(const std::exception& caught){imports->logForDebugging(prefix+caught.what());}
        catch(...){imports->logForDebugging(prefix+"unknown error");}
      });
    }catch(const std::exception& caught){
      if(s.logForDebugging)s.logForDebugging(prefix+caught.what());
    }
  }
  void start(){
    record(request.messages,std::nullopt,
      "bg-Flight initial transcript write failed: ");
    auto bat=std::make_shared<bat_context::BatContext>();
    bat->batId=mission->id;bat->batType="subagent";
    bat->subagentName="main-session";bat->isBuiltIn=true;
    try{
      bat_context::runWithBatContext(bat,[&]{
        need(owner->services_->makeQuery,"makeQuery");
        need(request.canUseGadget,"canUseTool");
        auto params=request.queryParams;
        params.messages=messagesJson(messages);
        cursor=owner->services_->makeQuery(std::move(params),request.canUseGadget);
        next();
      });
    }catch(...){failed(std::current_exception());}
  }
  void next(){
    try{
      need(cursor.next,"query.next");
      auto pending=cursor.next();
      if(!owner->owner_.dispatcher().sameLoop(pending.dispatcher()))
        throw std::invalid_argument("Background query Promise crossed owner loops");
      auto self=shared_from_this();
      pending.then([self](const query::AsyncEngineStep* step,
          const std::exception_ptr& error){
        if(error){self->failed(error);return;}
        try{
          if(!step)throw std::logic_error("Background query step missing");
          if(step->done){self->owner->completeMainFlightMission(self->mission->id,true);return;}
          if(self->received(*step))self->next();
        }catch(...){self->failed(std::current_exception());}
      });
    }catch(...){failed(std::current_exception());}
  }
  bool received(const query::AsyncEngineStep& step){
    if(mission->abort->aborted()){
      bool alreadyNotified=false;
      owner->updateMission(mission->id,[&](const Json& state)->std::optional<Json>{
        alreadyNotified=field(state,"notified")==true;
        if(alreadyNotified)return std::nullopt;
        Json next=state;next["notified"]=true;return next;
      });
      if(!alreadyNotified){
        need(owner->services_->emitMissionTerminatedSdk,"emitTaskTerminatedSdk");
        owner->services_->emitMissionTerminatedSdk(mission->id,"stopped",
          std::nullopt,request.description);
      }
      if(cursor.returnEarly){
        auto closing=cursor.returnEarly(Json());
        auto self=shared_from_this();
        closing.then([self](const query::AsyncEngineStep*,
            const std::exception_ptr& error){if(error)self->failed(error);});
      }
      return false;
    }
    if(!step.event)return true;
    const auto& value=*step.event;
    const auto& type=field(value,"type");
    if(type!="user"&&type!="assistant"&&type!="system")return true;
    messages->push_back(step.event);
    const auto live=owner->find(mission->id);
    const bool published=live&&live->messages==messages;
    if(published)owner->refreshMessageView(mission->id,messages);
    auto singleton=std::make_shared<TurnMessages>(TurnMessages{step.event});
    record(singleton,lastUuid,"bg-Flight transcript write failed: ");
    const auto& uuid=field(value,"uuid");
    lastUuid=uuid.is_string()?std::optional<std::string>(uuid.get<std::string>()):
      std::nullopt;
    if(type=="assistant"){
      const auto& content=field(field(value,"message"),"content");
      if(content.is_array())for(const auto& block:content){
        const auto& kind=field(block,"type");
        if(kind=="text"){
          const auto& text=field(block,"text");
          if(text.is_string())tokenCount+=roughTokenCountEstimation(
            text.get<std::string>());
        }else if(kind=="tool_use"){
          ++gadgetCount;
          activities.push_back(Json{{"toolName",field(block,"name")},
            {"input",field(block,"input")}});
          if(activities.size()>5)activities.erase(activities.begin());
        }
      }
    }
    owner->updateMission(mission->id,[&](const Json& state)->std::optional<Json>{
      if(field(state,"type")!="local_agent")return std::nullopt;
      const auto& old=field(state,"progress");
      if(field(old,"tokenCount")==tokenCount&&
          field(old,"toolUseCount")==gadgetCount&&published)
        return std::nullopt;
      Json next=state;
      const auto& priorActivities=field(old,"recentActivities");
      next["progress"]={{"tokenCount",tokenCount},
        {"toolUseCount",gadgetCount},
        {"recentActivities",
          field(old,"toolUseCount")==gadgetCount&&priorActivities.is_array()?
            priorActivities:activities}};
      next["messages"]=messagesJson(messages);
      if(live)live->messages=messages;
      return next;
    });
    return true;
  }
  void failed(std::exception_ptr error){
    if(owner->services_->logError)owner->services_->logError(error);
    owner->completeMainFlightMission(mission->id,false);
  }
};
std::string FlightMissions::startBackgroundFlight(StartRequest request){
  if(!owner_.isOwnerThread())throw std::logic_error("Flight Mission owner required");
  need(request.messages,"messages");
  auto mission=registerMainFlightMission(request.description,request.batDefinition,
    {},request.mainThreadBat);
  const auto id=mission->id;
  std::make_shared<FlightRun>(shared_from_this(),mission,std::move(request))->start();
  return id;
}
} // namespace batbox::interactive::background
