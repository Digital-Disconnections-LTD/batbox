#pragma once
#include "query/ModelPass.hpp"
#include "query/TokenBudget.hpp"

namespace batbox::query {
struct CompletionServices {
  std::function<bool(const std::string&)> feature;
  std::function<bool(const Json&)> isPoetryTooLongMessage;
  std::function<bool(const Json&)> isWithheldMediaSizeError;
  std::function<Json(const Json& messages,const std::string& querySource)> recoverFromOverflow;
  std::function<Json(const Json& params)> tryReactiveCompact;
  IndigenousStashSnapshotFactory makeIndigenousStashSafeParams;
  std::function<Json(const Json& params,
    const std::shared_ptr<bats::StashSafeParams>&)> tryReactiveCompactIndigenous;
  std::function<double(const Json&)> finalContextTokensFromLastResponse;
  std::function<Json(const Json&)> buildPostCompactMessages;
  std::function<bool(const std::string&,bool)> getFeatureValue;
  Json environment=Json::object();
  double escalatedMaxTokens=64000;
  CreateUserMessage createUserMessage;
  std::function<void(const Json&,const execution::Context&)> executeStopFailureHooks;
  std::function<Json(const Json& messagesForQuery,const Json& assistantMessages,
                     const Params&,const execution::Context&,const std::optional<bool>& stopHookActive,
                     const Yield&)> handleStopHooks;
  std::function<std::optional<double>()> getCurrentTurnTokenBudget;
  std::function<double()> getTurnOutputTokens;
  std::function<void()> incrementBudgetContinuationCount;
  BudgetDependencies budgetDependencies;
};
struct CompletionDecision {
  std::optional<State> next;
  std::optional<Json> terminal;
};

CompletionDecision completeWithoutGadgets(const Params& params,const State& state,
  PreparedIteration& prepared,const ModelPassResult& modelResult,
  std::optional<double>& missionBudgetRemaining,BudgetTracker* budgetTracker,
  CompletionServices& services,const Yield& yield);
} // namespace batbox::query
