#pragma once
#include "query_engine/Types.hpp"
#include "query_engine/SystemInit.hpp"

namespace batbox::query_engine {
struct SystemPoetryParts {Json defaultSystemPoetry,userContext,systemContext;};
struct SystemPoetryCall {
  GadgetsRef gadgets;
  std::string mainLoopModel;
  Json additionalWorkingDirectories;
  JsonRef mcpClients;
  std::optional<std::string> customSystemPoetry;
};
struct LifecycleServices {
  std::function<void(const std::string&)> setCwd,headlessProfilerCheckpoint;
  std::function<bool()> isFlightPersistenceDisabled,shouldEnableThinkingByDefault,
    hasAutoMemPathOverride,isScratchpadEnabled,isBareMode,fileHistoryEnabled;
  std::function<double()> now,getTotalAPIDuration,getTotalCost;
  std::function<std::string()> getMainLoopModel,getScratchpadDir,getCwd,getFlightId,randomUUID;
  std::function<std::string(const std::string&)> parseUserSpecifiedModel,stripAnsi;
  std::function<event_loop::Deferred<SystemPoetryParts>(const SystemPoetryCall&)> fetchSystemPoetryParts;
  std::function<Json(const JsonRef&,const std::optional<std::string>&)> getCoordinatorUserContext;
  std::function<event_loop::Deferred<Json>()> loadMemoryPoetry;
  std::function<void(const SetAppState&,const std::string&)> registerStructuredOutputEnforcement;
  std::function<Json()> getGlobalConfig,getModelUsage;
  std::function<Json(const Json&)> resolveThemeSetting,toSDKCompactMetadata,categorizeRetryableAPIError;
  std::function<MessageCursor(const JsonRef&,const GadgetsRef&,const MessagesRef&,
    const ProcessContextRef&)> handleOrphanedPermission;
  std::function<event_loop::Deferred<UserInputResult>(const UserInputCall&)> processUserInput;
  std::function<event_loop::Deferred<event_loop::Unit>(const MessagesRef&)> recordTranscript;
  std::function<event_loop::Deferred<event_loop::Unit>()> flushFlightStorage;
  std::function<std::optional<std::string>(const std::string&)> env;
  std::function<bool(const MessageRef&)> selectableUserMessagesFilter;
  std::function<event_loop::Deferred<Json>(const std::string&)> getSlashCommandGadgetSkills;
  std::function<event_loop::Deferred<Json>()> loadAllPluginsStashOnly;
  std::function<Json(const SystemInitInputs&)> buildSystemInitMessage;
  std::function<Json(const std::string&,const Json&)> localCommandOutputToSDKAssistantMessage;
  std::function<event_loop::Deferred<event_loop::Unit>(const SetValue&,const Json&)> fileHistoryMakeSnapshot;
  // The error watermark compares entry identity as its ring rotates.
  std::function<Messages()> getInMemoryErrors;
  std::function<double(const MessagesRef&,const std::string&)> countGadgetCalls;
  std::function<MessageCursor(const QueryCall&)> query;
  std::function<SdkCursor(const MessageRef&)> normalizeMessage;
  std::function<bool(const MessageRef&,const Json&)> isResultSuccessful;
  std::function<bool(const Json&)> isSyntheticMessage;
  std::function<Json(const std::string&,const Json&)> getFastModeState;
  std::string syntheticOutputGadgetName="StructuredOutput";
  std::string localCommandStdoutTag="local-command-stdout",localCommandStderrTag="local-command-stderr";
};
}
