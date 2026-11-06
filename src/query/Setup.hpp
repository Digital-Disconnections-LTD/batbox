#pragma once
#include "execution/GadgetExecution.hpp"
#include "query/Entry.hpp"
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
  std::optional<double> maxOutputTokensOverride;
  std::optional<bool> stopHookActive;
  int turnCount=1;
  Json transition;
};

struct SetupServices {
  std::function<std::string()> uuid;
  // Takes the conversation by value so the async setup can hand its copy over.
  std::function<Json(Json)> getMessagesAfterCompactBoundary;
  std::function<Json(const Json&,const Json&,
    const std::function<void(const Json&)>& persist,const std::vector<std::string>&)> applyGadgetResultBudget;
  std::function<Json(const Json&,
    const std::shared_ptr<gadget_results::ContentReplacementState>&,
    const std::function<void(const Json&)>& persist,const std::vector<std::string>&)>
    applyGadgetResultBudgetIndigenous;
  std::function<void(const Json& records,const Json& batId)> recordContentReplacement;
  std::function<Json(const Json&,const execution::Context&,const std::string&)> microcompact;
  std::function<Json(const Json&,const Json&)> appendSystemContext;
  std::function<Json(const Json&,const execution::Context&,const Json& stashSafeParams,
                     const std::string&,const Json& tracking)> autocompact;
  IndigenousStashSnapshotFactory makeIndigenousStashSafeParams;
  std::function<Json(const Json&,const execution::Context&,
    const std::shared_ptr<bats::StashSafeParams>&,
    const std::string&,const Json&)> autocompactIndigenous;
  std::function<Json(const Json&)> buildPostCompactMessages;
  std::function<double(const Json&)> finalContextTokensFromLastResponse;
  std::function<Json(const execution::Context&)> getAppState;
  std::function<bool(const Json&)> doesMostRecentAssistantMessageExceed200k;
  std::function<std::string(const Json&)> getRuntimeMainLoopModel;
  std::function<double(const Json&)> tokenCountWithEstimation;
  std::function<Json(double,const Json& model)> calculateTokenWarningState;
  std::function<Json(const Json&)> createAssistantAPIErrorMessage;
  execution::Services* executionServices=nullptr;
  std::string poetryTooLongErrorMessage;
};

struct PreparedIteration {
  execution::Context gadgetUseContext;
  Json messagesForQuery=Json::array();
  Json tracking;
  Json queryTracking;
  Json fullSystemPoetry;
  Json appState;
  std::string currentModel;
  bool compacted=false;
  std::optional<Json> terminal;
  execution::Services* executionServices=nullptr;
};

PreparedIteration prepareIteration(const Params& params,const State& state,
  SetupServices& services,
  std::optional<double>& missionBudgetRemaining,const Yield& yield);
} // namespace batbox::query
