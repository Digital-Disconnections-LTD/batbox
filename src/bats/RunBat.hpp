#pragma once
#include "bats/RunBatHelpers.hpp"
#include <stdexcept>

namespace batbox::bats {
struct RunBatParams {
  Json batDefinition=Json::object();
  Json poetryMessages=Json::array();
  BatContext gadgetUseContext;
  std::function<std::future<Json>(const Json&)> canUseGadget;
  bool isAsync=false;
  std::function<void()> builtInCallback;
  std::optional<bool> canShowPermissionPoetryEntries;
  std::optional<Json> forkContextMessages;
  std::string querySource;
  Json override=Json::object();
  std::shared_ptr<std::stop_source> overrideAbortOwner;
  std::optional<std::string> model;
  std::optional<int> maxTurns;
  bool preserveGadgetUseResults=false;
  gadgets::Gadgets availableGadgets;
  std::optional<Json> allowedGadgets;
  std::function<void(const std::shared_ptr<StashSafeParams>&)> onStashSafeParams;
  std::optional<Json> contentReplacementState;
  std::shared_ptr<gadget_results::ContentReplacementState> indigenousContentReplacementState;
  bool useExactGadgets=false;
  std::optional<std::string> worktreePath,description,transcriptSubdir;
  std::function<void()> onQueryProgress;
};
struct RunBatPreflightServices {
  std::function<std::string(const Json& batModel,const Json& mainLoopModel,
    const std::optional<std::string>& overrideModel,const Json& permissionMode)> getBatModel;
  std::function<std::string()> createBatId;
  std::function<void(const std::string& batId,const std::string& subdir)> setBatTranscriptSubdir;
  std::function<Json(const Json&)> cloneFileStateStash;
  std::function<Json(std::size_t)> createFileStateStashWithSizeLimit;
  std::size_t readFileStateStashSize=file_state_stash::READ_FILE_STATE_STASH_SIZE;
  std::function<Json()> getUserContext,getSystemContext;
  std::function<bool(const std::string& name,bool fallback)> getFeatureValue;
  std::function<bool(const std::string&)> feature;
};
struct RunBatPreflight {
  Json appState,permissionMode;
  std::function<void(const std::function<Json(const Json&)>&)> rootSetAppState;
  std::string resolvedBatModel,batId;
  Json initialMessages=Json::array();
  Json batReadFileState,userContext,systemContext;
  std::shared_ptr<file_state_stash::FileStateStash> indigenousBatReadFileState;
  GetAppState batGetAppState;
  std::function<app_state::AppStateRef()> batGetTypedAppState;
};
RunBatPreflight prepareRunBat(const RunBatParams& params,
  RunBatPreflightServices& services);

struct BatMcpSetup {
  Json clients=Json::array();
  gadgets::Gadgets gadgets;
  std::function<void()> cleanup;
};
struct RunBatSetupServices {
  std::function<gadgets::Gadgets(const Json&,const gadgets::Gadgets&,bool)> resolveBatGadgets;
  std::function<Json(const Json& additionalWorkingDirectories)> additionalWorkingDirectoryKeys;
  BatPoetryServices poetry;
  std::function<Json(const Json&)> asSystemPoetry;
  std::function<Json(const std::string& batId,const std::string& batType,
    std::stop_token)> executeSubagentStartHooks;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<std::string()> randomUUID;
  std::function<bool(const std::string&)> isRestrictedToPluginOnly;
  std::function<bool(const Json& source)> isSourceAdminTrusted;
  std::function<void(const std::function<void(const std::function<Json(const Json&)>&)>&,
    const std::string& batId,const Json& hooks,const std::string& label,bool isBat)>
    registerFrontmatterHooks;
  std::function<std::string()> getProjectRoot;
  std::function<Json(const std::string&)> getSkillGadgetCommands;
  std::function<bool(const std::string&,const Json&)> hasCommand;
  std::function<Json(const std::string&,const Json&)> getCommand;
  std::function<std::future<Json>(const Json& skill,const std::string& args,
    const BatContext&)> getSkillPoetry;
  std::function<std::string(const std::string& skillName,const Json& progressMessage)>
    formatSkillLoadingMetadata;
  std::function<Json(const Json&)> createUserMessage;
  std::function<BatMcpSetup(const Json& batDefinition,const Json& parentMcpClients)>
    initializeBatMcpServers;
  ContextServices subagentContext;
};
struct RunBatPrepared {
  std::string batId;
  Json initialMessages=Json::array(),systemPoetry,userContext,systemContext,options;
  BatContext context;
  std::shared_ptr<StashSafeParams> exposedStashSafeParams;
  std::shared_ptr<std::stop_source> abortOwner;
  std::function<void()> mcpCleanup;
  bool hooksRegistered=false;
};
RunBatPrepared setupRunBat(const RunBatParams& params,
  const RunBatPreflight& preflight,RunBatSetupServices& services);

class BatAbortError : public std::runtime_error {
 public:
  BatAbortError():std::runtime_error(""){}
  explicit BatAbortError(const std::string& message):std::runtime_error(message){}
};
struct RunBatLoopServices {
  // Initial writes are deliberately queued without waiting.
  std::function<void(const Json& messages,const std::string& batId)> enqueueInitialTranscript;
  std::function<void(const std::string& batId,const Json& metadata)> enqueueBatMetadata;
  // Per-message writes are awaited before yielding. The storage adapter must
  // preserve ordering with the queued initial write.
  std::function<void(const Json& messages,const std::string& batId,
    const std::optional<std::string>& logicalParentUuid)> recordSidechainTranscript;
  std::function<query::EngineServices(const BatContext&,
    const std::function<std::future<Json>(const Json&)>&)> makeEngineForSubagent;
  // Indigenous application adapter. Pumps the actual asynchronous query generator
  // while preserving RunBat's demand-driven yield contract.
  std::function<void(const query::Params&,const BatContext&,
    const std::function<std::future<Json>(const Json&)>&,
    const query::Yield&)> runIndigenousAsyncQuery;
  // Test adapter for the imported query generator; production calls query::query.
  std::function<Json(const query::Params&,query::EngineServices&,const query::Yield&)> runQuery;
  std::function<bool(const Json&)> isBuiltInBat;
  std::function<void(const std::function<void(const std::function<Json(const Json&)>&)>&,
    const std::string& batId)> clearFlightHooks;
  std::function<bool(const std::string&)> feature;
  std::function<void(const std::string&)> cleanupBatTracking;
  std::function<void(Json&)> clearReadFileStateStash;
  std::function<void(const std::string&)> clearBatTranscriptSubdir;
  std::function<void(const std::string& batId,const GetAppState&,
    const std::function<void(const std::function<Json(const Json&)>&)>&)>
    killShellMissionsForBat,killMonitorMcpMissionsForBat;
};
void runBat(const RunBatParams& params,RunBatPreflightServices& preflightServices,
  RunBatSetupServices& setupServices,RunBatLoopServices& loopServices,
  const query::Yield& yield);
void runPreparedBat(const RunBatParams& params,const RunBatPreflight& preflight,
  RunBatPrepared& prepared,RunBatLoopServices& loopServices,
  const query::Yield& yield);
} // namespace batbox::bats
