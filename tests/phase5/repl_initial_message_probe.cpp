#include "interactive/lifecycle/InitialMessage.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
Json run(const std::string& kind){
  event_loop::Loop loop;
  std::vector<std::string> trace;
  auto app=std::make_shared<app_state::AppState>();
  app->fields=std::make_shared<value::SharedRecord>();
  app->fields->set("initialMessage",{{"pending",true}});
  app->fields->set("fileHistory",{{"version",1}});
  app->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson({{"mode","default"}}));
  app->fields->set("toolPermissionContext",app->permissionContext->snapshot());
  auto store=std::make_shared<app_state::AppStateStore>(app,
    [&](const app_state::AppStateStore::Change& change){
      const auto initial=change.newState->fieldRef("initialMessage");
      const auto mode=change.newState->permissionContext->get("mode");
      trace.push_back(std::string("state:")+
        ((initial&&initial->is_null())?"true":"false")+":"+
        (mode&&mode->is_string()?mode->get<std::string>():"undefined"));
    });
  InitialMessageServices services{loop,store};
  bool haikuReset=true;
  std::size_t bashIndex=3;
  services.getPlanSlug=[&]{trace.push_back("slug:get");return "old-slug";};
  services.clearConversation=[&]{
    trace.push_back("clear");
    event_loop::Deferred<event_loop::Unit> done(loop);
    if(kind=="clear-failure")done.reject(std::make_exception_ptr(
      std::runtime_error("clear failed")));
    else done.resolve({});
    return done;
  };
  services.resetHaikuTitle=[&]{trace.push_back("title:undefined");haikuReset=false;};
  services.clearBashGadgets=[&]{trace.push_back("bash:clear");bashIndex=0;};
  services.getFlightId=[]{return "s2";};
  services.setPlanSlug=[&](const std::string& id,const std::string& slug){
    trace.push_back("slug:set:"+id+":"+slug);
  };
  services.buildPermissionUpdates=[&](const std::string& mode,const Json& poetryEntries){
    trace.push_back("build:"+mode+":"+std::to_string(poetryEntries.size()));
    return Json::array({{{"mode",mode}}});
  };
  services.applyPermissionUpdates=[&](const value::RecordRef& previous,
      const Json& updates){
    trace.push_back("apply");
    auto next=std::make_shared<value::SharedRecord>(previous->shallowCopy());
    next->set("mode",updates[0]["mode"]);return next;
  };
  services.feature=[](const std::string& name){return name=="TRANSCRIPT_CLASSIFIER";};
  services.stripDangerousPermissionsForAutoMode=[&](const value::RecordRef& context){
    const auto mode=context->get("mode");
    trace.push_back("strip:"+mode->get<std::string>());return context;
  };
  services.fileHistoryEnabled=[&]{return kind=="clear";};
  services.fileHistoryMakeSnapshot=[&](InitialMessageServices::HistorySetter setter,
      const std::string& uuid){
    trace.push_back("snapshot:"+uuid);
    setter([](const value::ValueRef& previous){
      auto next=std::make_shared<Json>(*previous);
      (*next)["version"]=(*previous)["version"].get<int>()+1;
      return next;
    });
    event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});return done;
  };
  services.awaitPendingHooks=[&]{
    trace.push_back("hooks");
    event_loop::Deferred<event_loop::Unit> done(loop);
    if(kind=="hook-failure")done.reject(std::make_exception_ptr(
      std::runtime_error("hook failed")));
    else done.resolve({});
    return done;
  };
  services.onSubmit=[&](const std::string& input){
    trace.push_back("submit:"+input);
    event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});return done;
  };
  services.setAbortController=[&](const std::shared_ptr<TurnAbort>&){
    trace.push_back("abort");
  };
  TurnMessageRef expected;
  services.onQuery=[&](TurnRequest request){
    trace.push_back(std::string("query:")+
      (request.newMessages->at(0)==expected?"true":"false")+":"+
      (request.shouldQuery?"true":"false")+":"+
      std::to_string(request.additionalAllowedGadgets.size())+":"+
      request.mainLoopModel);
    event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});return done;
  };
  services.mainLoopModel=[]{return "m";};
  services.onDetachedError=[&](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      trace.push_back(std::string("rejected:")+value.what());
    }
  };
  auto controller=std::make_shared<InitialMessageController>(std::move(services));
  auto message=std::make_shared<Json>(Json{{"type","user"},{"uuid","u1"},
    {"message",{{"content",kind=="complex"?Json::array({{{"type","image"}}}):Json("hello")}}}});
  const bool clear=kind=="clear"||kind=="clear-failure";
  if(kind=="plan"||clear)(*message)["planContent"]="plan";
  expected=message;
  InitialMessage initial{message,clear,std::nullopt,
    Json(Json::value_t::discarded)};
  if(clear){
    initial.mode="auto";
    initial.allowedPoetryEntries=Json::array({{{"prompt","tests"},{"tool","Bash"}}});
  }
  controller->observe(initial,false);
  controller->observe(initial,false); // the effect guard blocks a repeated render
  for(int index=0;index<8;++index)loop.runReady();
  if(kind=="timer"){
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    loop.runReady();
  }
  const auto state=store->getState();
  return Json{{"trace",trace},
    {"initialCleared",state->fieldRef("initialMessage")->is_null()},
    {"mode",state->permissionContext->get("mode")->get<std::string>()},
    {"historyVersion",state->fieldRef("fileHistory")->at("version")},
    {"processing",controller->processing()},
    {"haikuReset",haikuReset},{"bashIndex",bashIndex}};
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  for(const auto& kind:{"string","complex","plan","clear",
      "hook-failure","clear-failure","timer"})
    std::cout<<run(kind).dump()<<'\n';
}
