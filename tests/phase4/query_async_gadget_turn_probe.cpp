#include "query/AsyncGadgetTurn.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace batbox;
namespace {
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> out(loop);out.resolve(std::move(value));return out;
}
}
int main(int argc,char** argv){
  const std::string scenario=argc>1?argv[1]:"plain";
  event_loop::Loop loop;Json trace=Json::array();
  std::stop_source stop;
  if(scenario=="abort"||scenario=="submit_interrupt")stop.request_stop();
  Json gadgetBlock={{"type","tool_use"},{"name","Read"},{"id","tool-1"},
    {"input",{{"path","a"}}}};
  Json assistant={{"type","assistant"},{"uuid","assistant-1"},
    {"message",{{"content",Json::array({gadgetBlock})}}}};
  Json gadgetResult={{"type","user"},{"message",{{"content",Json::array({
    Json{{"type","tool_result"},{"tool_use_id","tool-1"},
      {"content","read done"}}})}}}};
  Json stopped={{"type","attachment"},
    {"attachment",{{"type","hook_stopped_continuation"}}}};
  Json attachment={{"type","attachment"},{"attachment",{{"type","queued"}}}};
  query::Params params;params.querySource="sdk";
  if(scenario=="max_turns")params.maxTurns=1;
  query::State state;state.turnCount=1;
  state.messages=Json::array({Json{{"type","user"},{"id","input"}}});
  auto prepared=std::make_shared<query::PreparedIteration>();
  prepared->messagesForQuery=state.messages;
  prepared->queryTracking={{"chainId","chain"},{"depth",0}};
  prepared->tracking={{"compacted",false},{"turnCounter",0}};
  prepared->gadgetUseContext.stop=stop.get_token();
  prepared->gadgetUseContext.data={{"options",{{"isNonInteractiveSession",false}}},
    {"agentId",nullptr}};
  auto result=std::make_shared<query::ModelPassResult>();
  result->assistantMessages=Json::array({assistant});
  result->gadgetUseBlocks=Json::array({gadgetBlock});result->needsFollowUp=true;
  query::QueryConfig config;
  config.emitGadgetUseSummaries=scenario=="summary";
  query::MemoryPrefetch memory;
  query::MemoryPrefetch* memoryPtr=nullptr;
  if(scenario=="memory"){
    memory.isSettled=[](){return true;};memory.consumedOnIteration=-1;
    memoryPtr=&memory;
  }
  if(scenario=="skill")prepared->pendingSkillPrefetch={{"id","skill"}};
  std::vector<std::string> consumed;
  query::AsyncGadgetTurnServices services;
  services.queryCheckpoint=[&](const std::string& name){
    trace.push_back(Json::array({"checkpoint",name}));};
  services.sync.feature=[&](const std::string& name){
    return name=="EXPERIMENTAL_SKILL_SEARCH"&&scenario=="skill";};
  services.gadgetUpdates=[&](const std::vector<Json>&,const Json&,
      const std::shared_ptr<query::PreparedIteration>& prepared,bool streaming){
    if(streaming)throw std::logic_error("unexpected streaming executor");
    trace.push_back("runTools");
    auto index=std::make_shared<int>(0);
    return query::AsyncGadgetUpdateCursor{[&,index,context=prepared->gadgetUseContext]{
      if((*index)++==0){
        query::AsyncGadgetUpdate update;
        update.message=std::make_shared<Json>(
          scenario=="hook_stopped"?stopped:gadgetResult);
        update.newContext=context;
        return ready(loop,std::optional<query::AsyncGadgetUpdate>(update));
      }
      return ready(loop,std::optional<query::AsyncGadgetUpdate>{});
    }};
  };
  services.sync.normalizeMessagesForAPI=[&](const Json& messages,const gadgets::Gadgets&){
    trace.push_back("normalize");return messages;};
  services.generateGadgetUseSummary=[&](const Json&,const execution::Context&){
    trace.push_back("generateSummary");
    return ready(loop,std::optional<std::string>("summary"));};
  services.sync.createGadgetUseSummaryMessage=[&](const std::string& summary,
      const Json& ids){
    trace.push_back("createSummary");
    return Json{{"type","summary"},{"summary",summary},{"ids",ids}};};
  services.sync.currentAbortReason=[&]{return scenario=="submit_interrupt"?
    std::string("interrupt"):std::string("timeout");};
  services.sync.createUserInterruptionMessage=[](bool gadgetUse){
    return Json{{"type","user"},{"interrupted",true},{"toolUse",gadgetUse}};};
  services.sync.createAttachmentMessage=[](const Json& value){
    return Json{{"type","attachment"},{"attachment",value}};};
  services.sync.getCommandsByMaxPriority=[&](const std::string& priority){
    trace.push_back(Json::array({"commands",priority}));
    return scenario=="commands"?Json::array({Json{{"mode","prompt"},
      {"uuid","q1"}}}):Json::array();};
  services.sync.isSlashCommand=[](const Json&){return false;};
  services.getAttachmentMessages=[&](const Json& commands,
      const execution::Context&,const Json&,const std::string&){
    trace.push_back(Json::array({"attachments",commands.size()}));
    auto index=std::make_shared<int>(0);
    return query::AsyncMessageCursor{[&,index]{
      if((*index)++==0&&(scenario=="attachments"||scenario=="commands"))
        return ready(loop,std::optional<model::MessageRef>(
          std::make_shared<Json>(attachment)));
      return ready(loop,std::optional<model::MessageRef>{});
    }};
  };
  services.readMemoryPrefetch=[&](query::MemoryPrefetch&){
    return ready(loop,Json::array({Json{{"kind","memory"}}}));};
  services.sync.filterDuplicateMemoryAttachments=[&](const Json& attachments,const Json&){
    trace.push_back("filterMemory");return attachments;};
  services.collectSkillDiscoveryPrefetch=[&](const Json&){
    trace.push_back("collectSkill");
    return ready(loop,Json::array({Json{{"kind","skill"}}}));};
  services.sync.notifyCommandLifecycle=[&](const std::string& id,
      const std::string& status){
    trace.push_back(Json::array({"lifecycle",id,status}));};
  services.sync.removeFromQueue=[&](const Json& commands){
    trace.push_back(Json::array({"remove",commands.size()}));};
  auto cursor=query::makeAsyncGadgetTurnCursor(loop,params,state,prepared,result,
    config,memoryPtr,consumed,services);
  std::shared_ptr<event_loop::Deferred<Json>> summaryPromise;
  while(true){
    auto pending=cursor.next();std::optional<query::AsyncDecisionStep> step;
    std::exception_ptr error;
    pending.then([&](const query::AsyncDecisionStep* value,const std::exception_ptr& rejected){
      error=rejected;if(value)step=*value;});
    loop.runReady();if(error)std::rethrow_exception(error);
    if(!step)throw std::logic_error("Async tool-turn next did not settle");
    if(step->done){
      if(!step->value)throw std::logic_error("Missing tool-turn decision");
      Json value;
      if(step->value->terminal)value=*step->value->terminal;
      else if(step->value->next){const auto& next=*step->value->next;
        summaryPromise=next.pendingGadgetUseSummaryAsync;
        value={{"transition",next.transition},{"messages",next.messages},
          {"turnCount",next.turnCount},{"toolResults",result->gadgetResults},
          {"pendingSummary",next.pendingGadgetUseSummaryAsync!=nullptr}};
      }else throw std::logic_error("Missing transition");
      trace.push_back(Json::array({"done",value}));break;
    }
    trace.push_back(Json::array({"yield",*step->event}));
  }
  if(scenario=="summary"){
    if(!summaryPromise)throw std::logic_error("Missing summary promise");
    std::optional<Json> summary;std::exception_ptr error;
    summaryPromise->then([&](const Json* value,const std::exception_ptr& rejected){
      error=rejected;if(value)summary=*value;});
    loop.runReady();if(error)std::rethrow_exception(error);
    if(!summary)throw std::logic_error("Summary did not settle");
    trace.push_back(Json::array({"pendingSummary",*summary}));
  }
  std::cout<<Json{{"trace",trace}}.dump()<<'\n';
}
