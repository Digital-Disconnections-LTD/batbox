#pragma once
#include "app_state/OnChange.hpp"
#include "query/Engine.hpp"
#include "services/api/batbox.hpp"
#include <memory>
#include <mutex>

namespace batbox::bats {
using GetAppState=std::function<Json()>;

struct BatContext {
  execution::Context query;
  GetAppState getAppState;
  std::function<void(const std::function<Json(const Json&)>&)> setAppState;
  std::function<void(const std::function<Json(const Json&)>&)> setAppStateForMissions;
  std::function<void(const Json&)> setResponseLength;
  std::function<void(const Json&)> setGadgetJSX;
  // The dispatcher passes a functional state updater from foreground Bat
  // progress. Keep it typed across Indigenous context copies.
  std::function<void(const std::function<Json(const Json&)>&)> updateResponseLength;
  std::function<void(const Json&)> pushApiMetricsEntry;
  std::function<void(const std::string&)> setInProgressGadgetUseIDs;
  std::function<void(const Json&)> updateFileHistoryState;
  std::function<void(const std::function<Json(const Json&)>&)> updateAttributionState;
  std::function<std::string()> currentAbortReason;
  std::shared_ptr<std::stop_source> abortOwner;
  std::shared_ptr<void> parentAbortLink;
  // Indigenous store callbacks preserve AppState and nested collection identity.
  // JSON callbacks above remain explicit boundaries for older gadget imports.
  std::function<app_state::AppStateRef()> getTypedAppState;
  std::function<void(const app_state::AppStateUpdater&)> setTypedAppState,
    setTypedAppStateForMissions;
  using TypedValueUpdater=std::function<value::ValueRef(const value::ValueRef&)>;
  std::function<void(const TypedValueUpdater&)> updateTypedFileHistoryState,
    updateTypedAttributionState;
};

// Materialize query-side live app-state wrappers before a BatContext is copied
// into a Bat, Gadget, or stash-safe snapshot. Clear the query sidecar so
// downstream query reads apply it exactly once.
void materializeAppStateModifier(BatContext& context);
GetAppState effectiveBatGetAppState(const BatContext& context);

struct StashSafeParams {
  Json systemPoetry=Json::array();
  Json userContext=Json::object();
  Json systemContext=Json::object();
  BatContext gadgetUseContext;
  Json forkContextMessages=Json::array();
};
StashSafeParams createStashSafeParams(const Json& hookContext,
  const BatContext& gadgetUseContext);
std::shared_ptr<StashSafeParams> makeStashSafeSnapshot(
  const std::shared_ptr<BatContext>& fullContext,
  const Json& systemPoetry,const Json& userContext,const Json& systemContext,
  const execution::Context& currentQueryContext,const Json& forkContextMessages);
void saveStashSafeParams(std::shared_ptr<StashSafeParams> params);
std::shared_ptr<StashSafeParams> getLastStashSafeParams();

GetAppState createGetAppStateWithAllowedGadgets(GetAppState baseGetAppState,
  const std::vector<std::string>& allowedGadgets);

struct PreparedForkedContext {
  std::string skillContent;
  GetAppState modifiedGetAppState;
  Json baseBat;
  Json poetryMessages=Json::array();
};
struct CommandServices {
  std::function<Json(const Json& command,const std::string& args,
                     const execution::Context&)> getPoetryForCommand;
  std::function<std::vector<std::string>(const Json&)> parseGadgetListFromCLI;
  std::function<Json(const Json&)> createUserMessage;
};
PreparedForkedContext prepareForkedCommandContext(const Json& command,
  const std::string& args,const execution::Context& context,
  const GetAppState& getAppState,CommandServices& services);
std::string extractResultText(const Json& batMessages,
  const std::string& defaultText="Execution completed");

struct SubagentOverrides {
  std::optional<Json> options;
  std::optional<gadgets::Gadgets> gadgets;
  std::optional<std::string> batId;
  std::optional<std::string> batType;
  std::optional<Json> messages;
  std::optional<Json> readFileState;
  std::shared_ptr<file_state_stash::FileStateStash> indigenousReadFileState;
  std::function<app_state::AppStateRef()> getTypedAppState;
  std::optional<std::stop_token> abortToken;
  std::function<std::string()> currentAbortReason;
  GetAppState getAppState;
  bool shareSetAppState=false;
  bool shareSetResponseLength=false;
  bool shareAbortController=false;
  std::optional<std::string> criticalSystemReminder;
  std::optional<bool> requireCanUseGadget;
  std::optional<Json> contentReplacementState;
  std::shared_ptr<gadget_results::ContentReplacementState> indigenousContentReplacementState;
};
struct ContextServices {
  std::function<Json(const Json&)> cloneFileStateStash;
  // Indigenous JSON-backed Map.clear adapter; optional instrumentation verifies
  // finalization ordering.
  std::function<void(Json&)> clearFileStateStash;
  std::function<Json(const Json&)> cloneContentReplacementState;
  std::function<Json()> createDenialTrackingState;
  std::function<std::string(const std::optional<std::string>&)> createBatId;
  std::function<std::string()> randomUUID;
};
BatContext createSubagentContext(const BatContext& parent,
  const SubagentOverrides& overrides,ContextServices& services);

struct ForkedBatParams {
  Json poetryMessages=Json::array();
  StashSafeParams stashSafeParams;
  std::function<std::future<Json>(const Json&)> canUseGadget;
  std::string querySource;
  std::string forkLabel;
  SubagentOverrides overrides;
  std::optional<double> maxOutputTokens;
  std::optional<int> maxTurns;
  std::function<void(const Json&)> onMessage;
  std::function<void(const std::shared_ptr<Json>&)> onMessageRef;
  bool skipTranscript=false;
  bool skipStashWrite=false;
};
struct ForkedBatResult {
  Json messages=Json::array();
  Json totalUsage=EMPTY_USAGE;
};
struct ForkServices {
  ContextServices context;
  std::function<query::EngineServices(const BatContext&,
    const std::function<std::future<Json>(const Json&)>&)> makeEngineForSubagent;
  // Test adapter for the imported query generator. Production leaves this
  // unbound and calls query::query directly.
  std::function<Json(const query::Params&,query::EngineServices&,const query::Yield&)> runQuery;
  std::function<void(const Json& messages,const std::string& batId,
                     const std::optional<std::string>& logicalParentUuid)> recordSidechainTranscript;
};
ForkedBatResult runForkedBat(const ForkedBatParams& params,
  ForkServices& services);
} // namespace batbox::bats
