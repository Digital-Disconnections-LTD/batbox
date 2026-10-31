#include "query/AsyncSetup.hpp"
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace bq=batbox::query;
namespace el=batbox::event_loop;

struct Watched {
  std::optional<bq::AsyncSetupStep> step;
  std::exception_ptr error;
};
static void require(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
static Json run(const std::string& scenario){
  const bool snip=scenario=="snip"||scenario=="snip_queued",
    compact=scenario=="compact",block=scenario=="block";
  el::Loop loop;
  el::Deferred<Json> budget(loop),micro(loop),collapse(loop),autocompact(loop);
  Json trace=Json::array();
  const Json messages=Json::array({Json{{"type","user"},{"id","original"}}});
  bq::Params params;
  params.messages=messages;
  params.querySource="sdk";
  params.systemPoetry=Json::array();
  params.gadgetUseContext.data={{"options",{{"mainLoopModel","model"}}}};
  bq::State state;
  state.messages=messages;
  state.gadgetUseContext=params.gadgetUseContext;
  bq::QueryConfig config;
  bq::AsyncSetupServices services;
  services.queryCheckpoint=[&](const std::string& name){
    trace.push_back(Json::array({"checkpoint",name}));};
  services.headlessProfilerCheckpoint=[&](const std::string& name){
    trace.push_back(Json::array({"headless",name}));};
  services.sync.feature=[&](const std::string& name){
    return name=="CONTEXT_COLLAPSE"||(snip&&name=="HISTORY_SNIP");};
  services.sync.uuid=[&]{trace.push_back("uuid");return std::string("chain");};
  services.sync.getMessagesAfterCompactBoundary=[&](const Json& value){
    trace.push_back("boundary");return value;};
  services.applyGadgetResultBudget=[&](const Json&,
      const std::shared_ptr<batbox::gadget_results::ContentReplacementState>&,
      const std::function<void(const Json&)>&,const std::vector<std::string>&){
    trace.push_back("budget");return budget;};
  services.sync.snipCompactIfNeeded=[&](const Json& value){
    trace.push_back("snip");return Json{{"messages",value},{"tokensFreed",3},
      {"boundaryMessage",{{"type","snip"}}}};};
  services.microcompact=[&](const Json&,const batbox::execution::Context&,
      const std::string&){trace.push_back("micro");return micro;};
  services.applyCollapsesIfNeeded=[&](const Json&,const batbox::execution::Context&,
      const std::string&){trace.push_back("collapse");return collapse;};
  services.sync.appendSystemContext=[](const Json& poetry,const Json&){return poetry;};
  services.autocompact=[&](const Json&,const batbox::execution::Context&,
      const Json&,const std::string&,const Json&,double){
    trace.push_back("auto");return autocompact;};
  services.sync.buildPostCompactMessages=[](const Json&){return Json::array({
    Json{{"type","post"},{"id",1}},Json{{"type","post"},{"id",2}}});};
  services.sync.getAppState=[](const batbox::execution::Context&){
    return Json{{"toolPermissionContext",{{"mode","default"}}}};};
  services.sync.getRuntimeMainLoopModel=[&](const Json&){
    trace.push_back("model");return std::string("model");};
  services.sync.isContextCollapseEnabled=[](){return false;};
  services.sync.tokenCountWithEstimation=[](const Json&){return 0.0;};
  services.sync.calculateTokenWarningState=[&](double,const Json&){
    return Json{{"isAtBlockingLimit",block}};};
  services.sync.createAssistantAPIErrorMessage=[](const Json&){
    return Json{{"type","blocking_error"}};};
  services.sync.poetryTooLongErrorMessage="too long";
  auto iterator=bq::makeAsyncSetupIterator(loop,params,state,config,services);
  auto watch=[&](el::Deferred<bq::AsyncSetupStep> pending){
    auto observed=std::make_shared<Watched>();
    pending.then([observed](const bq::AsyncSetupStep* value,const std::exception_ptr& error){
      if(error)observed->error=error;
      else observed->step=*value;
    });
    return observed;
  };
  auto record=[&](const std::shared_ptr<Watched>& observed){
    if(observed->error)std::rethrow_exception(observed->error);
    require(observed->step.has_value(),"next() did not settle");
    const auto& step=*observed->step;
    if(!step.done){trace.push_back(Json::array({"yield",step.event}));return;}
    if(!step.value){trace.push_back(Json::array({"done",nullptr}));return;}
    const auto& prepared=step.value->iteration;
    if(prepared.terminal){trace.push_back(Json::array({"done",
      prepared.terminal->at("reason")}));return;}
    trace.push_back(Json::array({"done",Json{{"messages",prepared.messagesForQuery},
      {"tracking",prepared.tracking.is_null()?Json():prepared.tracking},
      {"queryTracking",prepared.queryTracking},
      {"snipTokensFreed",prepared.snipTokensFreed},
      {"currentModel",prepared.currentModel}}}));
  };
  auto rejectAndFinish=[&](const std::shared_ptr<Watched>& pending,
      const std::string& expected){
    require(pending->error!=nullptr,"await did not reject");
    try{std::rethrow_exception(pending->error);}
    catch(const std::exception& error){
      require(error.what()==expected,"unexpected await rejection");
      trace.push_back(Json::array({"reject",error.what()}));
    }
    auto final=watch(iterator.next());loop.runReady();record(final);
    return Json{{"trace",trace}};
  };
  auto first=watch(iterator.next());
  std::shared_ptr<Watched> initialQueued;
  if(scenario=="initial_queued")initialQueued=watch(iterator.next());
  loop.runReady();record(first);
  if(!initialQueued)require(trace.size()==1,"budget started before second demand");
  auto next=initialQueued?initialQueued:watch(iterator.next());loop.runReady();
  std::shared_ptr<Watched> queued;
  if(scenario=="snip_queued")queued=watch(iterator.next());
  if(scenario=="reject_budget"){
    budget.reject(std::make_exception_ptr(std::runtime_error("budget-fail")));
    loop.runReady();return rejectAndFinish(next,"budget-fail");
  }
  budget.resolve(messages);loop.runReady();
  if(snip){record(next);next=queued?queued:watch(iterator.next());loop.runReady();}
  if(scenario=="reject_micro"){
    micro.reject(std::make_exception_ptr(std::runtime_error("micro-fail")));
    loop.runReady();return rejectAndFinish(next,"micro-fail");
  }
  micro.resolve(Json{{"messages",messages}});loop.runReady();
  if(scenario=="reject_collapse"){
    collapse.reject(std::make_exception_ptr(std::runtime_error("collapse-fail")));
    loop.runReady();return rejectAndFinish(next,"collapse-fail");
  }
  collapse.resolve(Json{{"messages",messages}});loop.runReady();
  if(scenario=="reject_auto"){
    autocompact.reject(std::make_exception_ptr(std::runtime_error("auto-fail")));
    loop.runReady();return rejectAndFinish(next,"auto-fail");
  }
  autocompact.resolve(Json{{"compactionResult",compact?
    Json{{"summaryMessages",Json::array()},{"attachments",Json::array()},
      {"hookResults",Json::array()}}:Json()}});loop.runReady();
  if(compact){
    record(next);
    auto second=watch(iterator.next());loop.runReady();record(second);
    auto done=watch(iterator.next());loop.runReady();record(done);
  }else if(block){
    record(next);
    auto done=watch(iterator.next());loop.runReady();record(done);
  }else record(next);
  return Json{{"trace",trace}};
}
int main(int argc,char** argv){
  try{std::cout<<batbox::value::stringify(run(argc>1?argv[1]:"plain"))<<'\n';}
  catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
