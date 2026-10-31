#include "interactive/QueryExecutionInternal.hpp"
#include "messages/TextContent.hpp"
#include <algorithm>

namespace batbox::interactive {
event_loop::Deferred<event_loop::Unit> executeQueryTurn(event_loop::Loop& owner,
    std::shared_ptr<QueryExecutionState> state,QueryExecutionParams params,
    std::shared_ptr<QueryExecutionServices> services){
  if(!owner.isOwnerThread())throw std::logic_error("Interactive query requires owner loop");
  if(!state||!services||!services->store||!params.messages||
      !params.request.newMessages||!params.request.abort)
    throw std::invalid_argument("Interactive query state, store and messages required");
  auto run=std::make_shared<query_execution::Run>(owner,std::move(state),
    std::move(params),std::move(services));
  try{run->start();}catch(...){run->result.reject(std::current_exception());}
  return run->result;
}
}
namespace batbox::interactive::query_execution {
void Run::start(){
  auto& s=*services;
  if(params.request.shouldQuery){
    need(s.mergeClients,"mergeClients");
    const auto current=s.store->getState()->snapshot();
    const auto clients=s.mergeClients(params.initialMcpClients,
      value::property(value::property(current,"mcp"),"clients"));
    need(s.diagnosticQueryStart,"diagnosticQueryStart");s.diagnosticQueryStart(clients);
    need(s.getConnectedIdeClient,"getConnectedIdeClient");
    const auto ide=s.getConnectedIdeClient(clients);
    if(value::truthy(ide)){need(s.closeOpenDiffs,"closeOpenDiffs");s.closeOpenDiffs(ide);}
  }
  need(s.markOnboardingComplete,"markOnboardingComplete");s.markOnboardingComplete();
  maybeTitle();applyAllowedGadgets();
  if(!params.request.shouldQuery){
    need(s.isCompactBoundary,"isCompactBoundary");
    if(std::any_of(params.request.newMessages->begin(),
        params.request.newMessages->end(),s.isCompactBoundary)){
      need(s.uuid,"uuid");need(s.setConversationId,"setConversationId");
      s.setConversationId(s.uuid());need(s.feature,"feature");
      if((s.feature("PROACTIVE")||s.feature("KAIROS"))&&s.setContextBlocked)
        s.setContextBlocked(false);
    }
    need(s.resetLoadingState,"resetLoadingState");s.resetLoadingState();
    need(s.clearAbortController,"clearAbortController");s.clearAbortController();
    result.resolve(event_loop::Unit{});return;
  }
  loadContext();
}
void Run::maybeTitle(){
  if(params.titleDisabled||value::truthy(params.flightTitle)||
      value::truthy(params.batTitle)||state->titleAttempted)return;
  TurnMessageRef first;
  for(const auto& message:*params.request.newMessages)
    if(value::property(*message,"type")=="user"&&!value::truthy(value::property(*message,"isMeta"))){
      first=message;break;
    }
  if(!first)return;
  const auto text=messages::getContentText(value::property(value::property(*first,"message"),"content"));
  if(!text||text->empty())return;
  for(const char* prefix:{"<local-command-stdout>","<command-message>",
      "<command-name>","<bash-input>"})if(text->starts_with(prefix))return;
  state->titleAttempted=true;
  need(services->generateFlightTitle,"generateSessionTitle");
  auto titleAbort=std::make_shared<TurnAbort>();
  auto pending=services->generateFlightTitle(*text,titleAbort);
  if(!owner.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Flight title awaited another owner loop");
  // This operation outlives the query; the query's abort signal is unrelated.
  auto titleState=state;auto imports=services;
  pending.then([titleState,imports,titleAbort](const Json* title,const std::exception_ptr& error){
    if(error||!title||!value::truthy(*title)){titleState->titleAttempted=false;return;}
    need(imports->setFlightTitle,"setSessionTitle");
    imports->setFlightTitle(title->get<std::string>());
  });
}
void Run::applyAllowedGadgets(){
  const Json allowed=params.request.additionalAllowedGadgets;
  services->store->setState([allowed](const app_state::AppStateRef& previous){
    const auto permission=previous->permissionContext;
    if(!permission)throw std::logic_error("Permission context required");
    const auto oldRules=permission->get("alwaysAllowRules");
    const auto current=oldRules?value::property(*oldRules,"command"):Json(Json::value_t::discarded);
    if(current==allowed)return previous;
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    auto nextPermission=std::make_shared<value::SharedRecord>(permission->shallowCopy());
    auto nextRules=oldRules?*oldRules:Json::object();nextRules["command"]=allowed;
    nextPermission->set("alwaysAllowRules",std::move(nextRules));
    next->setPermissionContext(std::move(nextPermission));return next;
  });
}
} // namespace batbox::interactive::query_execution
