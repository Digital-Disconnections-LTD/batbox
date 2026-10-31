#pragma once
#include "bats/BatGadgetBackgroundContinuation.hpp"
#include "bats/BatMissionForeground.hpp"

namespace batbox::bats {
struct BatGadgetForegroundFinalState {
  std::string syncBatId,description,poetry;
  BatMessageRefs batMessages;
  Json syncTracker=Json::object(),gadgetUseId,selectedBat;
  BatResultMetadata metadata;
  BatContext context;
  RootSetAppState rootSetAppState;
  std::optional<std::string> foregroundMissionId;
  std::function<void()> stopForegroundSummarization,cancelAutoBackground;
  std::shared_ptr<BatGadgetWorktreeState> worktree;
  std::function<Json()> cleanupWorktree;
  std::exception_ptr syncBatError;
  std::exception_ptr abortError;
  bool wasAborted=false,wasBackgrounded=false;
  std::int64_t batStartTime=0;
};
struct BatGadgetForegroundFinalServices {
  ForegroundServices* foreground=nullptr;
  std::function<void(const std::string&)> unregisterForeground; // fixture cleanup adapter
  BatGadgetWorktreeServices worktree;
  BatResultImports result;
  std::function<Json(const Json&)> getProgressUpdate;
  std::function<void(const Json&)> enqueueSdkEvent;
  std::function<void(const std::string&)> clearInvokedSkillsForBat;
  std::function<std::int64_t()> nowMillis;
  std::function<bool(const Json&)> isSyntheticMessage;
  std::function<bool(const std::string&)> feature;
  std::function<std::optional<std::string>(const Json&,
    const gadgets::Gadgets&,const Json&,std::stop_token,const std::string&,int)>
    classifyHandoffIfNeeded;
};
// Returns no result when the caller already returned an async_launched value
// from the background race. Otherwise performs finalization and completion.
std::optional<Json> finalizeBatGadgetForeground(BatGadgetForegroundFinalState& state,
  BatGadgetForegroundFinalServices& services);
} // namespace batbox::bats
