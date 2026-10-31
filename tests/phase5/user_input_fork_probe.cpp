#include "commands/fork/ForkCommand.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

using namespace batbox;
namespace {
Json assistant(const std::string& uuid,const std::string& text){
  return {{"type","assistant"},{"uuid",uuid},{"message",{{"content",
    Json::array({Json{{"type","text"},{"text",text}}})}}}};
}
Json messagesFor(int caseNumber){
  if(caseNumber==0)return Json::array({assistant("assistant-1","hello"),
    Json{{"type","user"},{"uuid","user-1"},{"message",{{"content",
      Json::array({Json{{"type","tool_result"},{"tool_use_id","t"},
        {"content","done"}}})}}}},assistant("assistant-2","final")});
  if(caseNumber==3)return Json::array({assistant("assistant-1","scheduled")});
  if(caseNumber==5)return Json::array({assistant("assistant-1","settled")});
  return Json::array();
}
Json project(const user_input::InputResult& result){
  Json messages=Json::array();
  for(const auto& item:result.messages)
    messages.push_back(value::property(value::property(item,"message"),"content"));
  return {{"messages",messages},{"shouldQuery",result.shouldQuery},
    {"resultText",result.resultText?Json(*result.resultText):Json(nullptr)}};
}
Json run(int caseNumber){
  event_loop::Loop loop;
  messages::Services messageServices;
  messageServices.randomUUID=[]{return "progress-id";};
  messageServices.nowISO=[]{return "now";};
  gadgets::Gadget read,grep;read.name="Read";grep.name="Grep";
  Json trace={{"progress",Json::array()},{"queued",Json::array()},
    {"errors",Json::array()},{"sleeps",0},{"refreshes",0},
    {"responseLength",0},{"params",nullptr}};
  const bool background=caseNumber>=3;
  const int pendingReads=caseNumber==5?2:caseNumber==6?100:0;
  int stateReads=0;
  int clockReads=0;
  auto appState=[&](){
    ++stateReads;
    return Json{{"kairosEnabled",background},{"mcp",{{"clients",
      pendingReads>0&&stateReads<=pendingReads+1?
        Json::array({Json{{"type","pending"}}}):
        Json::array()}}},{"toolPermissionContext",{{"alwaysAllowRules",
          {{"command",Json::array()}}}}}};
  };
  commands::fork::Services services{loop,messageServices};
  services.context.getAppState=appState;
  services.context.query.data={{"options",{{"agentDefinitions",{{"activeAgents",
    Json::array({Json{{"agentType","general-purpose"}}})}}}}}};
  services.context.query.gadgets={&read};
  services.context.updateResponseLength=[&](const auto& update){
    trace["responseLength"]=update(trace["responseLength"]);
  };
  services.prepare.getPoetryForCommand=[](const Json&,const std::string&,
    const execution::Context&){return Json::array({Json{{"type","text"},
      {"text","skill body"}}});};
  services.prepare.parseGadgetListFromCLI=[](const Json&){
    return std::vector<std::string>{};};
  services.createBatId=[]{return "agent-id";};
  services.randomUUID=[]{return "progress-id";};
  services.nowISO=[]{return "now";};
  services.nowMs=[&](){
    if(caseNumber==6)return std::int64_t(++clockReads*6000);
    return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();};
  services.feature=[background](const std::string& name){
    return name=="KAIROS"&&background;};
  services.refreshGadgets=[&](){trace["refreshes"]=trace["refreshes"].get<int>()+1;
    return gadgets::Gadgets{&read,&grep};};
  services.getWorkload=[](){return std::optional<Json>("cron");};
  services.enqueuePendingNotification=[&](const Json& item){trace["queued"].push_back(item);};
  services.showProgress=[&](const Json& progress){trace["progress"].push_back(progress.size());};
  services.clearProgress=[&](){trace["progress"].push_back("clear");};
  services.logError=[&](const std::string& error){trace["errors"].push_back(error);};
  services.runBat=[&](bats::RunBatParams params,
    commands::fork::BatMessage onMessage){
    event_loop::Deferred<event_loop::Unit> done(loop);
    trace["params"]={{"isAsync",params.isAsync},{"querySource",params.querySource},
      {"model",params.model?Json(*params.model):Json(nullptr)},
      {"agentId",value::property(params.override,"agentId")},
      {"tools",params.availableGadgets.size()},
      {"effort",value::property(params.batDefinition,"effort")},
      {"standaloneAbort",static_cast<bool>(params.overrideAbortOwner)}};
    loop.queueMicrotask([=]{
      if(caseNumber==2||caseNumber==4){
        done.reject(std::make_exception_ptr(std::runtime_error("agent failed")));
        return;
      }
      for(const auto& message:messagesFor(caseNumber))
        onMessage(std::make_shared<Json>(message));
      done.resolve(event_loop::Unit{});
    });
    return done;
  };
  Json command={{"name","commit"},{"userFacingName","commit"},
    {"effort","high"},{"model","fast"}};
  auto pending=commands::fork::executeForkedSlashCommand(command,"alpha",
    Json::array({Json{{"type","text"},{"text","prefix"}}}),services);
  pending.then([&](const user_input::InputResult* result,
    const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
      trace["output"]={{"error",e.what()}};}}
    else trace["output"]=project(*result);
  });
  loop.run();
  trace["sleeps"]=caseNumber==6?clockReads-2:
    std::max(0,stateReads-(background?2:1));
  return trace;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    try{std::cout<<value::stringify(run(std::stoi(line)))<<'\n';}
    catch(const std::exception& e){std::cout<<value::stringify(Json{{"fatal",e.what()}})<<'\n';}
  }
}
