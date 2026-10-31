#include "interactive/background/FlightMission.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::background;
struct Environment {
  event_loop::Loop loop;
  std::vector<std::string> events;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<FlightMissions> missions;
  std::vector<Json> streamEvents;
  int abortAtIndex=-1,throwAtIndex=-1,replaceMessagesAtIndex=-1;
  std::size_t stateUpdates=0;
  Environment(){
    auto initial=std::make_shared<app_state::AppState>();
    initial->fields=std::make_shared<value::SharedRecord>();
    initial->fields->set("tasks",Json::object());
    initial->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>();
    store=std::make_shared<app_state::AppStateStore>(initial);
    store->subscribe(std::make_shared<app_state::AppStateStore::Listener>(
      [this]{++stateUpdates;}));
    FlightMissionServices services;
    services.store=store;
    services.randomBytes8=[]{return std::array<unsigned char,8>{0,1,2,3,4,5,6,7};};
    auto clock=std::make_shared<int>(0);
    services.nowMillis=[clock]{return ++*clock==1?100:200;};
    services.getMissionOutputPath=[](const std::string& id){return "/output/"+id;};
    services.getBatTranscriptPath=[](const std::string& id){return "/transcript/"+id;};
    services.initMissionOutputAsSymlink=[this](const std::string& id,
        const std::string& path){events.push_back("symlink:"+id+":"+path);};
    services.registerCleanup=[this](const std::function<void()>&){
      return [this]{events.push_back("unregister");};
    };
    services.enqueueSdkEvent=[this](const Json& event){
      events.push_back("started:"+event.at("task_id").get<std::string>());
    };
    services.enqueuePendingNotification=[this](const Json& command){
      events.push_back("notification:"+command.at("value").get<std::string>());
    };
    services.emitMissionTerminatedSdk=[this](const std::string& id,
        const std::string& status,const std::optional<std::string>&,
        const std::string&){
      events.push_back("terminated:"+id+":"+status);
    };
    services.evictMissionOutput=[this](const std::string& id){
      events.push_back("evict:"+id);
    };
    services.recordSidechainTranscript=[this](const TurnMessagesRef& messages,
        const std::string& id,const std::optional<std::string>& previous){
      std::string uuids;
      for(const auto& message:*messages){
        if(!uuids.empty())uuids+=',';
        uuids+=message->at("uuid").get<std::string>();
      }
      events.push_back("transcript:"+id+":"+uuids+":"+previous.value_or(""));
      event_loop::Deferred<event_loop::Unit> value(loop);
      value.resolve({});return value;
    };
    services.logError=[this](std::exception_ptr error){
      try{std::rethrow_exception(error);}
      catch(const std::exception& caught){events.push_back(
        std::string("error:")+caught.what());}
    };
    services.makeQuery=[this](query::Params,const CanUseGadget&){
      auto index=std::make_shared<std::size_t>(0);
      query::AsyncQueryCursor cursor;
      cursor.next=[this,index]{
        event_loop::Deferred<query::AsyncEngineStep> value(loop);
        const auto i=static_cast<int>((*index)++);
        if(i==abortAtIndex)missions->find("s01234567")->abort->abort("stop");
        if(i==replaceMessagesAtIndex){
          auto foreign=std::make_shared<TurnMessages>(TurnMessages{
            std::make_shared<Json>(Json{{"uuid","foreign"}})});
          missions->find("s01234567")->messages=foreign;
          Json replacement=Json::array({Json{{"uuid","foreign"}}});
          (**store->getState()->missions->get("s01234567"))["messages"]=replacement;
          (*store->getState()->fields->get("tasks"))["s01234567"]["messages"]=
            std::move(replacement);
        }
        if(i==throwAtIndex){value.reject(std::make_exception_ptr(
          std::runtime_error("query failure")));return value;}
        query::AsyncEngineStep step;
        if(i>=static_cast<int>(streamEvents.size()))step.done=true;
        else step.event=std::make_shared<Json>(streamEvents.at(i));
        value.resolve(std::move(step));
        return value;
      };
      cursor.returnEarly=[this](const Json&){
        event_loop::Deferred<query::AsyncEngineStep> value(loop);
        query::AsyncEngineStep step;step.done=true;
        value.resolve(std::move(step));return value;
      };
      return cursor;
    };
    missions=std::make_shared<FlightMissions>(loop,std::move(services));
  }
  void edit(const std::string& id,const std::function<void(Json&)>& update){
    store->setState([&](const app_state::AppStateRef& previous){
      auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
      next->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>(
        *previous->missions);
      Json mission=**previous->missions->get(id);
      update(mission);
      next->missions->set(id,std::make_shared<Json>(mission));
      Json snapshot=previous->snapshot().at("tasks");
      snapshot[id]=mission;
      next->fields->set("tasks",std::move(snapshot));
      return next;
    });
  }
  Json mission(const std::string& id)const{
    return **store->getState()->missions->get(id);
  }
};
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  Environment e;
  auto mission=e.missions->registerMainFlightMission("job");
  const auto id=mission->id;
  const auto initial=Json{{"id",id},{"status",e.mission(id).at("status")},
    {"backgrounded",e.mission(id).at("isBackgrounded")},
    {"recognized",FlightMissions::isMainFlightMission(e.mission(id))}};
  mission->messages=std::make_shared<TurnMessages>(TurnMessages{
    std::make_shared<Json>(Json{{"type","assistant"},{"uuid","m1"}}),
    std::make_shared<Json>(Json{{"type","assistant"},{"uuid","m2"}})});
  e.edit(id,[](Json& value){value["toolUseId"]="tool";
    value["messages"]=Json::array({Json{{"type","assistant"},{"uuid","m1"}},
      Json{{"type","assistant"},{"uuid","m2"}}});});
  e.missions->completeMainFlightMission(id,true);
  e.missions->completeMainFlightMission(id,true);
  const auto complete=e.mission(id);
  std::cout<<Json{{"initial",initial},
    {"completed",{{"status",complete.at("status")},
      {"endTime",complete.at("endTime")},
      {"messages",Json::array({complete.at("messages")[0].at("uuid")})},
      {"notified",complete.at("notified")}}},
    {"events",e.events}}.dump()<<'\n';

  Environment f;
  auto abort=std::make_shared<TurnAbort>();
  auto other=f.missions->registerMainFlightMission("other",
    Json(Json::value_t::discarded),abort);
  other->messages=std::make_shared<TurnMessages>(TurnMessages{
    std::make_shared<Json>(Json{{"type","user"},{"uuid","f1"}})});
  f.edit(other->id,[](Json& value){value["messages"]=Json::array({
    Json{{"type","user"},{"uuid","f1"}}});});
  f.store->setState([](const app_state::AppStateRef& previous){
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>(
      *previous->missions);
    next->missions->set("prior",std::make_shared<Json>(Json{
      {"type","local_agent"},{"isBackgrounded",false}}));
    Json snapshot=previous->snapshot().at("tasks");
    snapshot["prior"]={{"type","local_agent"},{"isBackgrounded",false}};
    next->fields->set("tasks",snapshot);
    next->fields->set("foregroundedTaskId","prior");
    return next;
  });
  const auto returned=f.missions->foregroundMainFlightMission(other->id);
  f.missions->completeMainFlightMission(other->id,false);
  const auto done=f.mission(other->id);
  const auto previous=**f.store->getState()->missions->get("prior");
  std::cout<<Json{{"taskId",other->id},{"sameAbort",abort==other->abort},
    {"returned",Json::array({returned->at(0)->at("uuid")})},
    {"priorBackgrounded",previous.at("isBackgrounded")},
    {"foregroundedTaskId",*f.store->getState()->fieldRef("foregroundedTaskId")},
    {"taskBackgrounded",done.at("isBackgrounded")},
    {"status",done.at("status")},{"notified",done.at("notified")},
    {"events",f.events}}.dump()<<'\n';

  Environment replacement;
  const auto original=replacement.missions->registerMainFlightMission("first");
  replacement.edit(original->id,[](Json& value){
    value["retain"]=true;value["startTime"]=42;
    value["messages"]=Json::array({Json{{"uuid","old"}}});
    value["diskLoaded"]=true;
    value["pendingMessages"]=Json::array({"pending"});
  });
  const auto second=replacement.missions->registerMainFlightMission("second");
  const auto repeated=replacement.mission(original->id);
  std::size_t starts=0;
  for(const auto& event:replacement.events)if(event.starts_with("started:"))++starts;
  std::cout<<Json{{"description",repeated.at("description")},
    {"retain",repeated.at("retain")},{"startTime",repeated.at("startTime")},
    {"messages",Json::array({repeated.at("messages")[0].at("uuid")})},
    {"diskLoaded",repeated.at("diskLoaded")},
    {"pendingMessages",repeated.at("pendingMessages")},
    {"sameSelectedAgent",second->selectedBat==original->selectedBat},
    {"starts",starts}}.dump()<<'\n';

  const auto stream=std::vector<Json>{
    Json{{"type","system"},{"uuid","s1"}},
    Json{{"type","assistant"},{"uuid","a1"},
      {"message",{{"content",Json::array({
        Json{{"type","text"},{"text","12345678"}},
        Json{{"type","tool_use"},{"name","Search"},
          {"input",{{"q",1}}}}
      })}}}},
    Json{{"type","user"},{"uuid","u1"}}
  };
  for(const std::string mode:{"normal","aborted","failed","replaced"}){
    Environment run;
    run.streamEvents=stream;
    if(mode=="aborted")run.abortAtIndex=1;
    if(mode=="failed")run.throwAtIndex=1;
    if(mode=="replaced")run.replaceMessagesAtIndex=2;
    StartRequest request;
    request.messages=std::make_shared<TurnMessages>(TurnMessages{
      std::make_shared<Json>(Json{{"type","user"},{"uuid","start"}})});
    request.description="job";
    request.canUseGadget=[](const Json&){std::promise<Json> result;
      result.set_value(Json::object());return result.get_future();};
    const auto missionId=run.missions->startBackgroundFlight(std::move(request));
    for(int i=0;i<20;++i)run.loop.runReady();
    const auto value=run.mission(missionId);
    Json uuids=Json::array();
    const auto& messages=value::property(value,"messages");
    if(messages.is_array())for(const auto& message:messages)
      uuids.push_back(message.at("uuid"));
    const auto& progress=value::property(value,"progress");
    std::cout<<Json{{"mode",mode},{"id",missionId},
      {"status",value.at("status")},{"notified",value.at("notified")},
      {"progress",progress.is_null()?Json():progress},
      {"messages",uuids},{"stateUpdates",run.stateUpdates},
      {"events",run.events}}.dump()<<'\n';
  }
}
