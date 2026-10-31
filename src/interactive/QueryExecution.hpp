#pragma once
#include "app_state/State.hpp"
#include "interactive/QueryTurn.hpp"
#include "poetryEntries/EffectiveSystemPoetry.hpp"
#include "query/AsyncEntry.hpp"

namespace batbox::interactive {
struct QueryExecutionState {bool titleAttempted=false;};
struct InteractiveQueryContext {
  execution::Context context;
  std::shared_ptr<bats::BatContext> batContext;
};
struct QueryExecutionParams {
  TurnMessagesRef messages;
  TurnRequest request;
  Json initialMcpClients=Json::array();
  Json gadgetPermissionContext=Json::object();
  std::vector<std::string> additionalWorkingDirectories;
  bool titleDisabled=false;
  Json flightTitle=Json(Json::value_t::discarded);
  Json batTitle=Json(Json::value_t::discarded);
  poetryEntries::EffectivePoetryParams poetry;
};
struct QueryExecutionServices {
  std::shared_ptr<app_state::AppStateStore> store;
  std::function<Json(const Json&,const Json&)> mergeClients;
  std::function<void(const Json&)> diagnosticQueryStart,closeOpenDiffs;
  std::function<Json(const Json&)> getConnectedIdeClient;
  std::function<void()> markOnboardingComplete,resetLoadingState,clearAbortController,
    resetTurnHookDuration,resetTurnGadgetDuration,resetTurnClassifierDuration,
    logQueryProfileReport;
  std::function<event_loop::Deferred<Json>(const std::string&,
    const std::shared_ptr<TurnAbort>&)> generateFlightTitle;
  std::function<void(const std::string&)> setFlightTitle,setConversationId,checkpoint;
  std::function<bool(const std::string&)> feature;
  std::function<bool(const TurnMessageRef&)> isCompactBoundary;
  std::function<std::string()> uuid,getScratchpadDir,getQuerySource;
  std::function<void(bool)> setContextBlocked;
  std::function<InteractiveQueryContext(const TurnMessagesRef&,const TurnRequest&)>
    getGadgetUseContext;
  std::function<event_loop::Deferred<Json>(const Json&,
    const std::shared_ptr<app_state::AppStateStore>&)> checkBypassPermissions;
  std::function<event_loop::Deferred<Json>(const Json&,
    const std::shared_ptr<app_state::AppStateStore>&,const Json&)> checkAutoMode;
  std::function<event_loop::Deferred<Json>(const execution::Context&,
    const std::string&,const std::vector<std::string>&,const Json&)> getSystemPoetry;
  std::function<event_loop::Deferred<Json>()> getUserContext,getSystemContext;
  std::function<Json(const Json&,const std::optional<std::string>&)> getCoordinatorUserContext;
  std::function<bool()> scratchpadEnabled,isProactiveActive,terminalFocused;
  poetryEntries::EffectivePoetryServices effectivePoetry;
  std::function<query::AsyncQueryCursor(query::Params)> makeQuery;
  std::function<void(const query::AsyncEngineStep&)> onQueryEvent;
  std::function<TurnMessagesRef()> getMessages;
  std::function<void(const TurnMessagesRef&)> launchMonsterObserver;
  std::function<event_loop::Deferred<event_loop::Unit>(const TurnMessagesRef&)>
    onTurnComplete;
};
event_loop::Deferred<event_loop::Unit> executeQueryTurn(event_loop::Loop& owner,
  std::shared_ptr<QueryExecutionState> state,QueryExecutionParams params,
  std::shared_ptr<QueryExecutionServices> services);
// The application supplies the same engine factories used by the headless path.
void bindQueryExecutionEngine(event_loop::Loop& owner,QueryExecutionServices& services,
  std::shared_ptr<query::AsyncEngineServices> engine,query::NotifyLifecycle notify);
} // namespace batbox::interactive
