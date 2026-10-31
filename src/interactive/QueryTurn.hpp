#pragma once
#include "event_loop/Deferred.hpp"
#include "interactive/QueryGuard.hpp"
#include "runtime/JsonValue.hpp"
#include <stop_token>
#include <vector>

namespace batbox::interactive {
using TurnMessageRef=std::shared_ptr<Json>;
using TurnMessages=std::vector<TurnMessageRef>;
using TurnMessagesRef=std::shared_ptr<TurnMessages>;
using TurnMessageUpdate=std::function<TurnMessagesRef(const TurnMessagesRef&)>;
struct TurnAbort {
  std::stop_source controller;
  Json reason=Json(Json::value_t::discarded);
  void abort(Json why){
    if(controller.stop_requested())return;
    reason=std::move(why);controller.request_stop();
  }
  bool aborted()const{return controller.stop_requested();}
};
struct TurnRequest {
  TurnMessagesRef newMessages;
  std::shared_ptr<TurnAbort> abort;
  bool shouldQuery=true,proactiveActive=false;
  std::vector<std::string> additionalAllowedGadgets;
  std::string mainLoopModel;
  std::optional<std::string> input;
  Json effort=Json(Json::value_t::discarded);
  std::function<event_loop::Deferred<bool>(const std::string&,const TurnMessagesRef&)>
    onBeforeQuery;
};
struct QueryTurnState {
  QueryGuard guard;
  double loadingStartTime=0,totalPausedMs=0,lastQueryCompletionTime=0;
  bool skipIdleCheck=false;
  std::optional<double> swarmStartTime;
  std::optional<Json> swarmBudgetInfo;
};
struct QueryTurnServices {
  std::function<bool()> isBatSwarmsEnabled,hasRunningSwarmBats;
  std::function<std::optional<std::string>()> getTeamName,getBatName;
  std::function<void(const std::string&,const std::string&,bool)> setMemberActive;
  std::function<void(const Json&)> enqueue;
  std::function<void()> resetTimingRefs,clearApiMetrics,clearStreamingGadgetUses,
    clearStreamingText,resetLoadingState,sendBridgeResult,clearAbortController,
    removeLastFromHistory;
  std::function<void(double)> setResponseLengthRef;
  std::function<void(const TurnMessageUpdate&)> setMessages;
  std::function<TurnMessagesRef()> getMessages;
  std::function<bool(const std::string&)> feature;
  std::function<std::optional<double>(const std::string&)> parseTokenBudget;
  std::function<std::optional<double>()> getCurrentTurnTokenBudget;
  std::function<void(std::optional<double>)> snapshotOutputTokensForTurn;
  std::function<double()> now,getTurnOutputTokens,getBudgetContinuationCount;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&,
    const TurnMessagesRef&,std::size_t)> mrOnBeforeQuery;
  std::function<event_loop::Deferred<event_loop::Unit>(const TurnMessagesRef&,
    const TurnRequest&)> onQueryImpl;
  std::function<event_loop::Deferred<event_loop::Unit>(const TurnMessagesRef&,bool)>
    mrOnTurnComplete;
  std::function<bool(const TurnMessageRef&)> isLoggableMessage,selectableUserMessagesFilter;
  std::function<TurnMessageRef(double,const std::optional<Json>&,std::size_t)>
    createTurnDurationMessage;
  std::function<std::string()> getInputValue;
  std::function<std::size_t()> getCommandQueueLength;
  std::function<bool()> isViewingBatMission;
  std::function<bool(const TurnMessagesRef&,std::size_t)> messagesAfterAreOnlySynthetic;
  std::function<void(const TurnMessageRef&)> restoreMessageSync;
};
event_loop::Deferred<event_loop::Unit> runQueryTurn(event_loop::Loop& owner,
  std::shared_ptr<QueryTurnState> state,TurnRequest request,
  std::shared_ptr<QueryTurnServices> services);
} // namespace batbox::interactive
