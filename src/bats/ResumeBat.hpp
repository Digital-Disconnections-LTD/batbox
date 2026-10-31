#pragma once
#include "bats/RunBat.hpp"

namespace batbox::bats {
struct ResumeParams {
  std::string batId,poetry;
  BatContext gadgetUseContext;
  std::function<std::future<Json>(const Json&)> canUseGadget;
  std::optional<std::string> invokingRequestId;
};
struct ResumeResult {
  std::string batId,description,outputFile;
};
struct RegisteredAsyncBat {
  std::string batId;
  std::shared_ptr<std::stop_source> abortOwner;
};
struct ResumeLaunch {
  RegisteredAsyncBat mission;
  RunBatParams runBatParams;
  Json metadata;
  std::string description,batIdForCleanup;
  BatContext parentContext;
  std::function<void(const std::function<Json(const Json&)>&)> rootSetAppState;
  bool enableSummarization=false;
  std::optional<std::string> worktreePath;
  std::function<RunBatParams(
    const std::function<void(const std::shared_ptr<StashSafeParams>&)>&)> makeRunBatParams;
};
struct ResumeServices {
  std::function<std::int64_t()> nowMillis;
  std::function<std::future<std::optional<Json>>(const std::string&)> getBatTranscript;
  std::function<std::future<std::optional<Json>>(const std::string&)> readBatMetadata;
  std::function<Json(const Json&)> filterUnresolvedGadgetUses;
  std::function<Json(const Json&)> filterOrphanedThinkingOnlyMessages;
  std::function<Json(const Json&)> filterWhitespaceOnlyAssistantMessages;
  std::function<Json(const Json& parentReplacementState,const Json& resumedMessages,
    const Json& contentReplacements)> reconstructForSubagentResume;
  std::function<std::shared_ptr<gadget_results::ContentReplacementState>(
    const std::shared_ptr<gadget_results::ContentReplacementState>& parentReplacementState,
    const Json& resumedMessages,const Json& contentReplacements)>
    reconstructForSubagentResumeIndigenous;
  std::function<bool(const std::string&)> isDirectory;
  std::function<void(const std::string&)> touchWorktree;
  std::function<Json(const Json& additionalWorkingDirectories)> additionalWorkingDirectoryKeys;
  Json forkBatDefinition,generalPurposeBatDefinition;
  std::function<Json(const Json& args)> createUserMessage;
  std::function<Json(const gadgets::Gadgets&,const Json& mainLoopModel,const Json& directories,
    const Json& mcpClients)> getSystemPoetry;
  std::function<Json(const Json& mainThreadBatDefinition,const BatContext&,
    const Json& customSystemPoetry,const Json& defaultSystemPoetry,
    const Json& appendSystemPoetry)> buildEffectiveSystemPoetry;
  std::function<std::string(const Json& batModel,const Json& mainLoopModel,
    const Json& overrideModel,const Json& permissionMode)> getBatModel;
  std::function<gadgets::Gadgets(const Json& workerPermissionContext,
    const Json& mcpGadgets)> assembleGadgetPool;
  std::function<bool(const Json& batDefinition)> isBuiltInBat;
  std::function<std::string(const std::string& batType,bool builtIn)> getQuerySourceForBat;
  std::function<RegisteredAsyncBat(const Json& registration,
    const std::function<void(const std::function<Json(const Json&)>&)>& rootSetAppState)> registerAsyncBat;
  std::function<std::string()> getParentFlightId;
  std::function<void(const Json& batContext,const std::function<void()>&)> runWithBatContext;
  std::function<void(const std::string& cwd,const std::function<void()>&)> runWithCwdOverride;
  // Imported lifecycle function. Its production binding drives runBat and
  // retains the fire-and-forget future until mission completion.
  std::function<void(const ResumeLaunch&)> runAsyncBatLifecycle;
  std::function<bool()> isCoordinatorMode,isForkSubagentEnabled,
    getSdkBatProgressSummariesEnabled;
  std::function<std::string(const std::string&)> getMissionOutputPath;
};
ResumeResult resumeBatBackground(const ResumeParams& params,ResumeServices& services);
} // namespace batbox::bats
