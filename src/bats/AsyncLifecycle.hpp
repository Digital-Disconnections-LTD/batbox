#pragma once
#include "bats/BatGadgetResults.hpp"
#include "bats/RunBat.hpp"

namespace batbox::bats {
using RootSetAppState=std::function<void(const std::function<Json(const Json&)>&)>;
using ActivityResolver=std::function<std::optional<std::string>(
  const std::string& gadgetName,const Json& input)>;
using BatMessageRefs=std::vector<query::MessageRef>;
struct AsyncLifecycleParams {
  std::string missionId,description,batIdForCleanup;
  std::shared_ptr<std::stop_source> abortOwner;
  std::function<void(
    const std::function<void(const std::shared_ptr<StashSafeParams>&)>& onStashSafeParams,
    const query::Yield& yield)> makeStream;
  BatResultMetadata metadata;
  BatContext gadgetUseContext;
  RootSetAppState rootSetAppState;
  bool enableSummarization=false;
  std::function<Json()> getWorktreeResult;
};
struct AsyncLifecycleImports {
  BatResultImports result;
  std::function<Json()> createProgressTracker;
  std::function<ActivityResolver(const gadgets::Gadgets&)> createActivityDescriptionResolver;
  std::function<std::function<void()>(const std::string& missionId,
    const std::shared_ptr<StashSafeParams>&,const RootSetAppState&)>
    startBatSummarization;
  std::function<bool(const Json&)> isLocalBatMission;
  std::function<void(Json& tracker,const Json& message,const ActivityResolver&,
    const gadgets::Gadgets&)> updateProgressFromMessage;
  std::function<Json(const Json& tracker)> getProgressUpdate;
  std::function<void(const std::string& missionId,const Json& progress,
    const RootSetAppState&)> updateAsyncBatProgress;
  std::function<void(const Json&)> emitMissionProgressEvent;
  std::function<double(const Json& tracker)> getTokenCountFromTracker;
  std::function<void(const Json& result,const BatMessageRefs&,
    const RootSetAppState&)> completeAsyncBat;
  std::function<bool(const std::string&)> feature;
  std::function<std::optional<std::string>(const Json& batMessages,
    const gadgets::Gadgets&,const Json& permissionContext,std::stop_token,
    const std::string& batType,int totalGadgetUseCount)> classifyHandoffIfNeeded;
  std::function<void(const Json& notification,const RootSetAppState&)> enqueueBatNotification;
  std::function<void(const std::string& missionId,const BatMessageRefs&,
    const RootSetAppState&)> killAsyncBat;
  std::function<void(const std::string& missionId,const std::string& error,
    const BatMessageRefs&,const RootSetAppState&)> failAsyncBat;
  std::function<std::string(std::exception_ptr)> errorMessage;
  std::function<void(const std::string&)> clearInvokedSkillsForBat;
};
void runAsyncBatLifecycle(const AsyncLifecycleParams& params,
  AsyncLifecycleImports& imports);
} // namespace batbox::bats
