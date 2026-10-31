#include "interactive/QueryTurn.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace ix=batbox::interactive;
int main(int argc,char** argv){
  const std::string scenario=argc>1?argv[1]:"normal";
  event_loop::Loop loop;
  auto state=std::make_shared<ix::QueryTurnState>();state->skipIdleCheck=true;
  auto services=std::make_shared<ix::QueryTurnServices>();auto& s=*services;
  auto messages=std::make_shared<ix::TurnMessages>();
  auto first=std::make_shared<Json>(Json{{"type","user"},{"uuid","u"},
    {"message",{{"content","hello"}}}});
  ix::TurnRequest request;
  request.abort=std::make_shared<ix::TurnAbort>();request.shouldQuery=scenario!="local";
  request.newMessages=std::make_shared<ix::TurnMessages>(ix::TurnMessages{first});
  request.additionalAllowedGadgets={"Read"};request.mainLoopModel="model";
  request.proactiveActive=scenario=="proactive";
  if(scenario!="no_input")request.input="hello";
  const bool budgetEnabled=scenario.starts_with("budget");
  std::optional<double> budget=budgetEnabled?std::optional<double>(40):std::nullopt;
  Json trace=Json::array(),metrics=Json::array({1});double responseLength=5;
  const auto unit=[&]{event_loop::Deferred<event_loop::Unit> p(loop);p.resolve({});return p;};
  event_loop::Deferred<event_loop::Unit> gate(loop);
  s.isBatSwarmsEnabled=[&]{return scenario=="swarm";};
  s.getTeamName=[]{return std::optional<std::string>("team");};
  s.getBatName=[]{return std::optional<std::string>("friend");};
  s.setMemberActive=[&](const auto& team,const auto& bat,bool active){trace.push_back(Json::array({"active",team,bat,active}));};
  s.enqueue=[&](const Json& value){trace.push_back(Json::array({"enqueue",value}));};
  s.resetTimingRefs=[&]{trace.push_back("timing");};
  s.setMessages=[&](const ix::TurnMessageUpdate& update){
    messages=update(messages);Json ids=Json::array();
    for(const auto& m:*messages)ids.push_back(m->value("uuid",m->value("type","")));
    trace.push_back(Json::array({"messages",ids}));
  };
  s.getMessages=[&]{return messages;};
  s.setResponseLengthRef=[&](double value){responseLength=value;};
  s.feature=[&](const std::string& name){return name=="TOKEN_BUDGET"&&budgetEnabled;};
  s.parseTokenBudget=[&](const std::string&){return std::optional<double>(scenario=="budget_zero"?0:10);};
  s.getCurrentTurnTokenBudget=[&]{return budget;};
  s.snapshotOutputTokensForTurn=[&](std::optional<double> value){
    budget=value;trace.push_back(Json::array({"budget",value?Json(*value):Json()}));};
  s.clearApiMetrics=[&]{metrics=Json::array();};
  s.clearStreamingGadgetUses=[&]{trace.push_back(Json::array({"tools",Json::array()}));};
  s.clearStreamingText=[&]{trace.push_back(Json::array({"text",nullptr}));};
  s.mrOnBeforeQuery=[&](const std::string& input,const ix::TurnMessagesRef& msgs,std::size_t count){
    trace.push_back(Json::array({"before",input,msgs->size(),count}));
    auto out=unit();
    if(scenario=="before_error"){
      event_loop::Deferred<event_loop::Unit> failed(loop);
      failed.reject(std::make_exception_ptr(std::runtime_error("before failed")));return failed;
    }return out;
  };
  s.onQueryImpl=[&](const ix::TurnMessagesRef& msgs,const ix::TurnRequest& call){
    trace.push_back(Json::array({"query",msgs->size(),call.newMessages->size(),call.shouldQuery,
      call.additionalAllowedGadgets,call.mainLoopModel,nullptr}));
    if(scenario=="impl_error"||scenario=="cleanup_error"){
      event_loop::Deferred<event_loop::Unit> failed(loop);
      failed.reject(std::make_exception_ptr(std::runtime_error("query failed")));return failed;
    }
    if(scenario.starts_with("cancel")||scenario=="resubmit_stale"){
      call.abort->abort(scenario=="cancel_background"?"background":"user-cancel");state->guard.forceEnd();
      if(scenario=="resubmit_stale")state->guard.tryStart();
      if(scenario=="cancel_meaningful")messages->push_back(std::make_shared<Json>(Json{
        {"type","assistant"},{"uuid","a"},{"message",{{"content","answer"}}}}));
    }return gate;
  };
  s.now=[&]{return scenario.starts_with("long")?40001.0:100.0;};
  s.resetLoadingState=[&]{trace.push_back(Json::array({"completed",state->lastQueryCompletionTime}));trace.push_back("reset");};
  s.mrOnTurnComplete=[&](const ix::TurnMessagesRef& msgs,bool aborted){
    trace.push_back(Json::array({"turnComplete",msgs->size(),aborted}));
    if(scenario=="cleanup_error"){
      event_loop::Deferred<event_loop::Unit> failed(loop);
      failed.reject(std::make_exception_ptr(std::runtime_error("cleanup failed")));return failed;
    }return unit();
  };
  s.sendBridgeResult=[&]{trace.push_back("bridge");};
  s.getTurnOutputTokens=[]{return 17.0;};s.getBudgetContinuationCount=[]{return 2.0;};
  s.hasRunningSwarmBats=[&]{return scenario=="long_swarm";};
  s.isLoggableMessage=[](const ix::TurnMessageRef&){return true;};
  s.createTurnDurationMessage=[](double duration,const std::optional<Json>& budget,std::size_t count){
    auto value=std::make_shared<Json>(Json{{"type","system"},{"duration",duration},{"count",count}});
    if(budget)(*value)["budget"]=*budget;return value;
  };
  s.clearAbortController=[&]{trace.push_back(Json::array({"abort",nullptr}));};
  s.getInputValue=[&]{return scenario=="cancel_typed"?"typing":"";};
  s.getCommandQueueLength=[&]{return scenario=="cancel_queue"?1U:0U;};
  s.isViewingBatMission=[&]{return scenario=="cancel_viewing";};
  s.selectableUserMessagesFilter=[](const ix::TurnMessageRef& msg){
    return msg->value("type","")=="user"&&!msg->value("isMeta",false);};
  s.messagesAfterAreOnlySynthetic=[](const ix::TurnMessagesRef& msgs,std::size_t index){
    for(++index;index<msgs->size();++index)if((*msgs)[index]->value("type","")=="assistant")return false;return true;};
  s.removeLastFromHistory=[&]{trace.push_back("history");};
  s.restoreMessageSync=[&](const ix::TurnMessageRef& msg){trace.push_back(Json::array({"restore",msg->at("uuid")}));};
  if(scenario=="concurrent"){
    state->guard.tryStart();auto meta=std::make_shared<Json>(*first);(*meta)["uuid"]="meta";(*meta)["isMeta"]=true;
    request.newMessages->push_back(meta);
    request.newMessages->push_back(std::make_shared<Json>(Json{{"type","assistant"},{"uuid","a"}}));
    request.newMessages->push_back(std::make_shared<Json>(Json{{"type","user"},{"uuid","empty"},{"message",{{"content",""}}}}));
  }
  if(scenario=="before_denied")request.onBeforeQuery=[&](const std::string&,const ix::TurnMessagesRef&){
    trace.push_back("approval");event_loop::Deferred<bool> out(loop);out.resolve(false);return out;};
  auto pending=ix::runQueryTurn(loop,state,request,services);gate.resolve({});
  pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){trace.push_back(Json::array({"error",e.what()}));}}
    else trace.push_back("resolved");
  });
  loop.runReady();if(!pending.settled())throw std::runtime_error("Turn did not settle");
  Json all=Json::array();for(const auto& msg:*messages)all.push_back(*msg);
  Json refs={{"start",0},{"paused",0},{"completed",state->lastQueryCompletionTime},
    {"skip",state->skipIdleCheck},{"swarm",state->swarmStartTime?Json(*state->swarmStartTime):Json()}};
  if(state->swarmBudgetInfo)refs["swarmBudget"]=*state->swarmBudgetInfo;
  std::cout<<Json{{"trace",trace},{"active",state->guard.isActive()},{"generation",state->guard.generation()},
    {"messages",all},{"refs",refs},{"metrics",metrics},{"responseLength",responseLength}}.dump()<<'\n';
}
