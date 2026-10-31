#pragma once
#include "bats/BatMissionProgress.hpp"

namespace batbox::bats {
struct BatMissionStateServices {
  RootSetAppState setAppState;
  GetAppState getAppState;
  std::function<std::int64_t()> nowMillis;
  std::function<void(const std::string&)> abortMission,unregisterCleanup,
    evictMissionOutput;
  std::function<bool()> sdkProgressSummariesEnabled;
  std::function<void(const Json&)> emitMissionProgress;
  std::int64_t panelGraceMs=30000;
};
bool isLocalBatMission(const Json& mission);
bool isPanelBatMission(const Json& mission);
void queuePendingMessage(const std::string& missionId,const std::string& message,
  BatMissionStateServices& services);
void appendMessageToLocalBat(const std::string& missionId,const Json& message,
  BatMissionStateServices& services);
Json drainPendingMessages(const std::string& missionId,BatMissionStateServices& services);
void killAsyncBat(const std::string& missionId,BatMissionStateServices& services,
  const BatMessageRefs* refs=nullptr);
void killAllRunningBatMissions(const Json& missions,BatMissionStateServices& services);
void markBatsNotified(const std::string& missionId,BatMissionStateServices& services);
void updateBatProgress(const std::string& missionId,const Json& progress,
  BatMissionStateServices& services);
void updateBatSummary(const std::string& missionId,const std::string& summary,
  BatMissionStateServices& services);
void completeBatMission(const Json& result,BatMissionStateServices& services,
  const BatMessageRefs* refs=nullptr);
void failBatMission(const std::string& missionId,const std::string& error,
  BatMissionStateServices& services,const BatMessageRefs* refs=nullptr);
} // namespace batbox::bats
