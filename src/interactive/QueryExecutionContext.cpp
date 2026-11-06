#include "interactive/QueryExecutionInternal.hpp"
#include "query/OwnerPromiseAll.hpp"

namespace batbox::interactive::query_execution {
void Run::loadContext(){
  auto& s=*services;need(s.getGadgetUseContext,"getToolUseContext");
  gadgetContext=s.getGadgetUseContext(params.messages,params.request);
  freshMcpClients=value::property(value::property(gadgetContext.context.data,"options"),"mcpClients");
  if(!params.request.effort.is_discarded()){
    const auto effort=params.request.effort;
    execution::appendAppStateModifierPair(gadgetContext.context,
      [effort](const Json& original){
        auto state=original;state["effortValue"]=effort;return state;
      },[effort](const app_state::AppStateRef& original){
        if(!original)throw std::logic_error("Typed app state required for effort override");
        auto state=std::make_shared<app_state::AppState>(original->shallowCopy());
        state->fields->set("effortValue",effort);return state;
      });
  }
  need(s.checkpoint,"checkpoint");s.checkpoint("query_context_loading_start");
  std::vector<event_loop::Deferred<Json>> inputs;
  need(s.checkBypassPermissions,"checkBypassPermissions");
  inputs.push_back(s.checkBypassPermissions(params.gadgetPermissionContext,s.store));
  need(s.getSystemPoetry,"getSystemPrompt");
  inputs.push_back(s.getSystemPoetry(gadgetContext.context,params.request.mainLoopModel,
    params.additionalWorkingDirectories,freshMcpClients));
  need(s.getUserContext,"getUserContext");inputs.push_back(s.getUserContext());
  need(s.getSystemContext,"getSystemContext");inputs.push_back(s.getSystemContext());
  auto self=shared_from_this();
  await(query::ownerPromiseAll(owner,inputs),
    [self](const std::vector<Json>& values){self->contextReady(values);});
}
void Run::contextReady(const std::vector<Json>& values){
  auto& s=*services;
  auto userContext=values.at(2);
  s.checkpoint("query_context_loading_end");
  auto poetry=params.poetry;poetry.gadgetUseContext=gadgetContext.context;
  poetry.defaultSystemPoetry=values.at(1);
  const auto systemPoetry=poetryEntries::buildEffectiveSystemPoetry(poetry,s.effectivePoetry);
  gadgetContext.context.data["renderedSystemPrompt"]=systemPoetry;
  s.checkpoint("query_query_start");
  need(s.resetTurnHookDuration,"resetTurnHookDuration");s.resetTurnHookDuration();
  need(s.resetTurnGadgetDuration,"resetTurnToolDuration");s.resetTurnGadgetDuration();
  need(s.resetTurnClassifierDuration,"resetTurnClassifierDuration");s.resetTurnClassifierDuration();
  query::Params query;
  for(const auto& message:*params.messages)query.messages.push_back(*message);
  query.systemPoetry=systemPoetry;query.userContext=std::move(userContext);
  query.systemContext=values.at(3);query.gadgetUseContext=gadgetContext.context;
  query.indigenousBatContext=gadgetContext.batContext;
  need(s.getQuerySource,"getQuerySource");query.querySource=s.getQuerySource();
  need(s.makeQuery,"makeQuery");cursor=s.makeQuery(std::move(query));next();
}
} // namespace batbox::interactive::query_execution
