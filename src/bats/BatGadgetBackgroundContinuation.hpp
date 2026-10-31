#pragma once
#include "bats/BatGadgetAsync.hpp"
#include "bats/BatGadgetStream.hpp"
#include "bats/BatMissionProgress.hpp"

namespace batbox::bats {
struct BatGadgetBackgroundContinuation {
  std::string missionId,syncBatId,description;
  Json gadgetUseId,selectedBat;
  BatResultMetadata metadata;
  RunBatParams runBatParams;
  BatContext context;
  RootSetAppState rootSetAppState;
  BatMessageRefs existingMessages;
  std::shared_ptr<BatGadgetStream> foregroundStream;
  // Fixture adapter for iterator.return(); production uses foregroundStream.
  std::function<void()> returnForegroundIterator;
  std::shared_ptr<std::stop_source> missionAbortOwner;
  std::shared_ptr<BatGadgetWorktreeState> worktree;
  // Fixture adapter for the cleanup closure.
  std::function<Json()> cleanupWorktree;
};
struct BatGadgetBackgroundServices {
  BatGadgetStream::Runner runBat;
  BatResultImports result;
  BatMissionProgressImports progress;
  BatGadgetWorktreeServices worktree;
  std::function<void(const Json& tracker,const std::string& missionId,
    const Json& gadgetUseId,const std::string& description,
    std::int64_t startTime,const std::string& lastGadgetName)> emitMissionProgress;
  std::function<void(const std::string&,const Json&,const RootSetAppState&)>
    updateAsyncBatProgress;
  std::function<bool()> getSdkBatProgressSummariesEnabled;
  std::function<std::function<void()>(const std::string& missionId,
    const std::string& batId,const std::shared_ptr<StashSafeParams>&,
    const RootSetAppState&)> startBatSummarization;
  std::function<void(const Json&,const BatMessageRefs&,const RootSetAppState&)>
    completeAsyncBat;
  std::function<void(const std::string&,const BatMessageRefs&,
    const RootSetAppState&)> killAsyncBat;
  std::function<void(const std::string&,const std::string&,
    const BatMessageRefs&,const RootSetAppState&)> failAsyncBat;
  std::function<void(const Json&,const RootSetAppState&)> enqueueBatNotification;
  std::function<bool(const std::string&)> feature;
  std::function<std::optional<std::string>(const Json&,const gadgets::Gadgets&,
    const Json&,std::stop_token,const std::string&,int)> classifyHandoffIfNeeded;
  std::function<std::string(std::exception_ptr)> errorMessage;
  std::function<void(const std::string&)> clearInvokedSkillsForBat;
};
void continueBatGadgetInBackground(BatGadgetBackgroundContinuation state,
  BatGadgetBackgroundServices& services);
} // namespace batbox::bats
