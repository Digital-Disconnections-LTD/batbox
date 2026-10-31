#include "interactive/background/NotificationAttachments.hpp"
#include "interactive/background/FlightMission.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::background;
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  event_loop::Loop loop;
  auto state=std::make_shared<app_state::AppState>();
  state->fields=std::make_shared<value::SharedRecord>();
  state->fields->set("tasks",Json::object());
  state->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>();
  auto store=std::make_shared<app_state::AppStateStore>(state);
  std::vector<Json> notifications;
  FlightMissionServices missionImports;
  missionImports.store=store;
  missionImports.randomBytes8=[]{return std::array<unsigned char,8>{0,1,2,3,4,5,6,7};};
  missionImports.nowMillis=[]{return 100;};
  missionImports.getMissionOutputPath=[](const std::string& id){return "/output/"+id;};
  missionImports.getBatTranscriptPath=[](const std::string& id){return "/transcript/"+id;};
  missionImports.initMissionOutputAsSymlink=[](const std::string&,const std::string&){};
  missionImports.registerCleanup=[](const std::function<void()>&){return []{};};
  missionImports.enqueueSdkEvent=[](const Json&){};
  missionImports.enqueuePendingNotification=[&](const Json& value){
    notifications.push_back(value);
  };
  missionImports.emitMissionTerminatedSdk=[](const std::string&,
    const std::string&,const std::optional<std::string>&,const std::string&){};
  missionImports.evictMissionOutput=[](const std::string&){};
  missionImports.recordSidechainTranscript=[&](const TurnMessagesRef&,
      const std::string&,const std::optional<std::string>&){
    event_loop::Deferred<event_loop::Unit> value(loop);value.resolve({});return value;
  };
  missionImports.makeQuery=[&](query::Params,const CanUseGadget& permission){
    assert(permission);
    auto count=std::make_shared<int>(0);
    query::AsyncQueryCursor cursor;
    cursor.next=[&loop,count]{
      event_loop::Deferred<query::AsyncEngineStep> result(loop);
      query::AsyncEngineStep step;
      if((*count)++==0)step.event=std::make_shared<Json>(Json{
        {"type","assistant"},{"uuid","answer"},
        {"message",{{"content",Json::array({Json{{"type","text"},
          {"text","done"}}})}}}});
      else step.done=true;
      result.resolve(std::move(step));return result;
    };
    cursor.returnEarly=[&loop](const Json&){
      event_loop::Deferred<query::AsyncEngineStep> result(loop);
      query::AsyncEngineStep step;step.done=true;
      result.resolve(std::move(step));return result;
    };
    return cursor;
  };
  auto missions=std::make_shared<FlightMissions>(loop,std::move(missionImports));
  submit::CommandQueue queue(loop);
  queue.enqueue(Json{{"mode","task-notification"},{"value","notice"}});
  auto messages=std::make_shared<TurnMessages>(TurnMessages{
    std::make_shared<Json>(Json{{"type","user"},{"uuid","start"}})});
  auto services=std::make_shared<BackgroundQueryServices>();
  services->queue=&queue;
  services->foregroundAbort=std::make_shared<TurnAbort>();
  services->getMessages=[messages]{return messages;};
  services->getGadgetUseContext=[](const TurnMessagesRef&,
      const TurnRequest&){
    InteractiveQueryContext result;
    result.context.data={{"options",{{"mcpClients",Json::array()}}}};
    return result;
  };
  services->getSystemPoetry=[&](const execution::Context&,
      const std::string&,const std::vector<std::string>&,const Json&){
    event_loop::Deferred<Json> result(loop);
    result.resolve(Json::array({"system"}));return result;
  };
  services->getUserContext=[&]{event_loop::Deferred<Json> result(loop);
    result.resolve(Json::object());return result;};
  services->getSystemContext=services->getUserContext;
  services->getQueuedCommandAttachments=[&](const submit::CommandList& commands){
    return getQueuedCommandAttachments(loop,commands,AttachmentServices{});
  };
  services->createAttachmentMessage=[](const Json& value){
    return std::make_shared<Json>(Json{{"type","attachment"},
      {"uuid","attached"},{"attachment",value}});
  };
  services->startBackgroundFlight=[missions](StartRequest request){
    missions->startBackgroundFlight(std::move(request));
  };
  services->getQuerySource=[]{return "repl";};
  services->getAdditionalWorkingDirectories=[](){return std::vector<std::string>{};};
  services->canUseGadget=[](const Json&){std::promise<Json> result;
    result.set_value(Json::object());return result.get_future();};
  services->onDetachedError=[](std::exception_ptr error){std::rethrow_exception(error);};
  services->effectivePoetry.feature=[](const std::string&){return false;};
  services->mainLoopModel="model";
  services->terminalTitle="job";
  handleBackgroundQuery(loop,services);
  for(int i=0;i<30;++i)loop.runReady();
  const auto* mission=store->getState()->missions->get("s01234567");
  assert(mission&&*mission);
  assert((**mission).at("status")=="completed");
  assert((**mission).at("messages")[0].at("uuid")=="answer");
  assert(services->foregroundAbort->aborted());
  assert(queue.empty());
  assert(notifications.size()==1);
  std::cout<<"1 native queue→context→attachment→query→task composition case passed\n";
}
