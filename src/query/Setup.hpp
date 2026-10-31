#pragma once
#include "execution/StreamingExecutor.hpp"
#include "query/Config.hpp"
#include "query/Entry.hpp"
#include <future>
#include <memory>
#include <optional>

namespace batbox::bats { struct BatContext; struct StashSafeParams; }
namespace batbox::event_loop { template<class T> class Deferred; }

namespace batbox::query {
using IndigenousStashSnapshotFactory=std::function<std::shared_ptr<bats::StashSafeParams>(
  const std::shared_ptr<bats::BatContext>&,
  const Json& systemPoetry,const Json& userContext,const Json& systemContext,
  const execution::Context&,const Json& forkContextMessages)>;
struct Params {
  Json messages=Json::array();
  Json systemPoetry=Json::array();
  Json userContext=Json::object();
  Json systemContext=Json::object();
  execution::Context gadgetUseContext;
  std::optional<std::string> fallbackModel;
  std::string querySource;
  std::optional<double> maxOutputTokensOverride;
  std::optional<int> maxTurns;
  bool skipStashWrite=false;
  std::optional<double> missionBudgetTotal;
  std::shared_ptr<bats::BatContext> indigenousBatContext;
};

struct State {
  Json messages=Json::array();
  execution::Context gadgetUseContext;
  Json autoCompactTracking;
  int maxOutputTokensRecoveryCount=0;
  bool hasAttemptedReactiveCompact=false;
  std::optional<double> maxOutputTokensOverride;
  std::optional<std::shared_future<Json>> pendingGadgetUseSummary;
  std::shared_ptr<event_loop::Deferred<Json>> pendingGadgetUseSummaryAsync;
  std::optional<bool> stopHookActive;
  int turnCount=1;
  Json transition;
};

struct SetupServices {
  std::function<bool(const std::string&)> feature;
  std::function<std::string()> uuid;
  std::function<Json(const Json&)> getMessagesAfterCompactBoundary;
  std::function<Json(const Json&,const Json&,
    const std::function<void(const Json&)>& persist,const std::vector<std::string>&)> applyGadgetResultBudget;
  std::function<Json(const Json&,
    const std::shared_ptr<gadget_results::ContentReplacementState>&,
    const std::function<void(const Json&)>& persist,const std::vector<std::string>&)>
    applyGadgetResultBudgetIndigenous;
  std::function<void(const Json& records,const Json& batId)> recordContentReplacement;
  std::function<Json(const Json&)> snipCompactIfNeeded;
  std::function<Json(const Json&,const execution::Context&,const std::string&)> microcompact;
  std::function<Json(const Json&,const execution::Context&,const std::string&)> applyCollapsesIfNeeded;
  std::function<Json(const Json&,const Json&)> appendSystemContext;
  std::function<Json(const Json&,const execution::Context&,const Json& stashSafeParams,
                     const std::string&,const Json& tracking,double snipTokensFreed)> autocompact;
  IndigenousStashSnapshotFactory makeIndigenousStashSafeParams;
  std::function<Json(const Json&,const execution::Context&,
    const std::shared_ptr<bats::StashSafeParams>&,
    const std::string&,const Json&,double)> autocompactIndigenous;
  std::function<Json(const Json&)> buildPostCompactMessages;
  std::function<double(const Json&)> finalContextTokensFromLastResponse;
  std::function<Json(const execution::Context&)> getAppState;
  std::function<bool(const Json&)> doesMostRecentAssistantMessageExceed200k;
  std::function<std::string(const Json&)> getRuntimeMainLoopModel;
  std::function<bool()> isAutoCompactEnabled;
  std::function<bool()> isReactiveCompactEnabled;
  std::function<bool()> isContextCollapseEnabled;
  std::function<double(const Json&)> tokenCountWithEstimation;
  std::function<Json(double,const Json& model)> calculateTokenWarningState;
  std::function<Json(const Json&)> createAssistantAPIErrorMessage;
  std::function<Json(const Json&,const execution::Context&)> startSkillDiscoveryPrefetch;
  execution::Services* executionServices=nullptr;
  std::string poetryTooLongErrorMessage;
};

struct PreparedIteration {
  execution::Context gadgetUseContext;
  Json messagesForQuery=Json::array();
  Json tracking;
  Json queryTracking;
  Json pendingSkillPrefetch;
  Json pendingStashEdits;
  Json fullSystemPoetry;
  Json appState;
  std::string currentModel;
  double snipTokensFreed=0;
  bool mediaRecoveryEnabled=false;
  bool compacted=false;
  std::optional<Json> terminal;
  std::unique_ptr<execution::StreamingGadgetExecutor> streamingGadgetExecutor;
  std::vector<std::unique_ptr<execution::StreamingGadgetExecutor>> discardedExecutors;
  execution::Services* executionServices=nullptr;
};

PreparedIteration prepareIteration(const Params& params,const State& state,
  const QueryConfig& config,SetupServices& services,
  std::optional<double>& missionBudgetRemaining,const Yield& yield);
} // namespace batbox::query
