#pragma once
#include "query/Setup.hpp"
#include "query/Recovery.hpp"
#include <chrono>

namespace batbox::query {
using HookResultSink=std::function<bool(const Json&)>;
struct StopHookServices {
  std::function<bool(const std::string&)> feature;
  std::function<Json(const Json&)> createStashSafeParams;
  std::function<void(const Json&)> saveStashSafeParams;
  std::function<void(const std::string& jobDir,const Json& assistantMessages,
                     std::chrono::milliseconds deadline)> classifyAndWriteJobState;
  std::function<bool()> isBareMode;
  std::function<bool(const Json&)> isEnvDefinedFalsy;
  std::function<void(const Json&)> executePoetrySuggestion;
  std::function<bool()> isExtractModeActive;
  std::function<void(const Json&)> executeExtractMemories;
  std::function<void(const Json&)> executeAutoDream;
  std::function<void(const execution::Context&)> cleanupComputerUseAfterTurn;
  std::function<Json(const execution::Context&)> getAppState;
  std::function<void(const Json& args,const execution::Context&,
    const HookResultSink&)> executeStopHooks;
  std::function<Json(const Json&)> getStopHookMessage;
  CreateUserMessage createUserMessage;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<Json(bool gadgetUse)> createUserInterruptionMessage;
  std::function<Json(int hookCount,const Json& infos,const Json& errors,bool prevented,
                     const std::string& stopReason,bool hasOutput,
                     const std::string& suggestion,const std::string& gadgetUseID)> createStopHookSummaryMessage;
  std::function<std::string()> getShortcutDisplay;
  std::function<void(const Json&)> addNotification;
  std::function<bool()> isPartner;
  std::function<std::optional<std::string>()> getBatName;
  std::function<std::optional<std::string>()> getTeamName;
  std::function<std::string()> getMissionListId;
  std::function<Json(const std::string&)> listMissions;
  std::function<void(const Json& args,const HookResultSink&)> executeMissionCompletedHooks;
  std::function<Json(const Json&)> getMissionCompletedHookMessage;
  std::function<void(const Json& args,const HookResultSink&)> executePartnerIdleHooks;
  std::function<Json(const Json&)> getPartnerIdleHookMessage;
  std::function<Json(const std::string&,const std::string&)> createSystemMessage;
  Json environment=Json::object();
};

Json handleStopHooks(const Json& messagesForQuery,const Json& assistantMessages,
  const Params& params,const execution::Context& context,
  std::optional<bool> stopHookActive,StopHookServices& services,const Yield& yield);
} // namespace batbox::query
