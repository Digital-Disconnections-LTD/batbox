#include "query/AsyncCompletion.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace batbox;
namespace {
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> out(loop);out.resolve(std::move(value));return out;
}
Json normalizedDecision(const query::CompletionDecision& value,
  const std::optional<double>& remaining){
  if(value.terminal)return *value.terminal;
  if(!value.next)throw std::logic_error("Missing completion transition");
  const auto& next=*value.next;
  return {{"transition",next.transition},{"messages",next.messages},
    {"maxOutputTokensRecoveryCount",next.maxOutputTokensRecoveryCount},
    {"hasAttemptedReactiveCompact",next.hasAttemptedReactiveCompact},
    {"maxOutputTokensOverride",next.maxOutputTokensOverride?
      Json(*next.maxOutputTokensOverride):Json()},
    {"stopHookActive",next.stopHookActive?Json(*next.stopHookActive):Json()},
    {"taskBudgetRemaining",remaining?Json(*remaining):Json()}};
}
}
int main(int argc,char** argv){
  const std::string scenario=argc>1?argv[1]:"plain";
  const bool ptl=scenario.starts_with("ptl_"),media=scenario=="media_no_recovery";
  const bool max=scenario.starts_with("max_");
  event_loop::Loop loop;Json trace=Json::array();
  query::Params params;params.querySource="sdk";
  if(scenario=="ptl_reactive")params.missionBudgetTotal=100;
  query::State state;
  state.messages=Json::array({Json{{"type","user"},{"id","input"}}});
  state.maxOutputTokensRecoveryCount=scenario=="max_exhausted"?3:0;
  auto prepared=std::make_shared<query::PreparedIteration>();
  prepared->messagesForQuery=state.messages;
  prepared->tracking={{"compacted",false}};
  prepared->mediaRecoveryEnabled=media;
  auto result=std::make_shared<query::ModelPassResult>();
  Json assistant={{"type","assistant"},{"uuid","assistant"},
    {"message",{{"content",Json::array()}}}};
  if(ptl||media||max)assistant["isApiErrorMessage"]=true;
  if(max)assistant["apiError"]="max_output_tokens";
  auto assistantRef=std::make_shared<Json>(assistant);
  result->assistantMessages=Json::array({assistant});result->assistantRefs={assistantRef};
  std::optional<double> remaining;
  query::BudgetDependencies budgetDeps;
  budgetDeps.nowMillis=[](){return std::int64_t{0};};
  budgetDeps.continuationMessage=[](double,double,double){return std::string("keep going");};
  query::BudgetTracker tracker;
  query::AsyncCompletionServices services;
  auto& sync=services.sync;
  sync.feature=[scenario](const std::string& name){
    return name=="CONTEXT_COLLAPSE"&&scenario=="ptl_collapse"||
      name=="TOKEN_BUDGET"&&scenario=="budget_continue";};
  sync.isPoetryTooLongMessage=[ptl](const Json&){return ptl;};
  sync.isWithheldMediaSizeError=[media](const Json&){return media;};
  sync.recoverFromOverflow=[&](const Json&,const std::string&){
    trace.push_back("collapse");return Json{{"committed",2},
      {"messages",Json::array({Json{{"type","summary"}}})}};};
  services.contextCollapseAvailable=[scenario]{return scenario=="ptl_collapse";};
  services.reactiveCompactAvailable=[scenario]{return scenario=="ptl_reactive"||
    scenario=="ptl_no_recovery"||scenario=="media_no_recovery";};
  services.tryReactiveCompact=[&](const Json&,
      const std::shared_ptr<bats::StashSafeParams>&){
    trace.push_back("reactive");
    return ready(loop,scenario=="ptl_reactive"?Json{{"summary","compact"}}:Json());
  };
  sync.finalContextTokensFromLastResponse=[&](const Json&){
    trace.push_back("contextTokens");return 20.0;};
  sync.buildPostCompactMessages=[&](const Json&){
    trace.push_back("post");return Json::array({Json{{"type","post"},{"id",1}},
      Json{{"type","post"},{"id",2}}});};
  sync.createUserMessage=[&](const Json& args){
    trace.push_back("createUser");return Json{{"type","user"},{"args",args}};};
  sync.executeStopFailureHooks=[&](const Json&,const execution::Context&){
    trace.push_back("failureHooks");};
  sync.getFeatureValue=[scenario](const std::string& name,bool){
    return name=="batbox_output_token_escalation"&&scenario=="max_escalate";};
  sync.escalatedMaxTokens=64000;
  const Json stopMessage={{"type","user"},{"content","blocked"}};
  services.handleStopHooks=[&](const Json&,const Json&,const query::Params&,
      const execution::Context&,std::optional<bool>){
    trace.push_back("stopHooks");
    auto index=std::make_shared<int>(0);
    return query::AsyncStopCursor{[&,index]{
      if(scenario=="stop_block"&&(*index)++==0)
        return ready(loop,query::AsyncStopStep{false,
          std::make_shared<Json>(stopMessage),{}});
      Json output={{"blockingErrors",scenario=="stop_block"?
        Json::array({stopMessage}):Json::array()},
        {"preventContinuation",scenario=="stop_prevent"}};
      return ready(loop,query::AsyncStopStep{true,{},output});
    }};
  };
  sync.budgetDependencies=budgetDeps;
  sync.getCurrentTurnTokenBudget=[](){return std::optional<double>{1000};};
  sync.getTurnOutputTokens=[](){return 200.0;};
  sync.incrementBudgetContinuationCount=[&]{trace.push_back("increment");};
  services.logForDebugging=[&](const std::string& message){
    trace.push_back(Json::array({"debug",message}));};
  services.localeNumber=[](double value){return value==1000?std::string("1,000"):
    value::numberToString(value);};
  auto cursor=query::makeAsyncCompletionCursor(loop,params,state,prepared,result,
    remaining,&tracker,services);
  while(true){
    auto pending=cursor.next();std::optional<query::AsyncDecisionStep> step;
    std::exception_ptr error;
    pending.then([&](const query::AsyncDecisionStep* value,const std::exception_ptr& rejected){
      error=rejected;if(value)step=*value;});
    loop.runReady();if(error)std::rethrow_exception(error);
    if(!step)throw std::logic_error("Async completion next did not settle");
    if(step->done){
      trace.push_back(Json::array({"done",normalizedDecision(*step->value,remaining)}));
      break;
    }
    trace.push_back(Json::array({"yield",*step->event}));
  }
  std::cout<<Json{{"trace",trace}}.dump()<<'\n';
}
