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
// Settled Bat records accumulate in AppState for the whole Flight. Once an
// unobserved record's evictAfter grace has passed it is reduced to the fields
// the Bats list shows; dismissed records (evictAfter 0) and settled records
// beyond the newest keepSettled are removed. busy(id) protects records whose
// follow-up input or resume is still in flight.
struct SettledBatSweep {
  std::optional<Json> missions;      // replacement tasks object, if changed
  std::optional<std::int64_t> nextDueMs; // earliest pending evictAfter
  std::size_t summarized=0,removed=0;
};
inline constexpr std::size_t settledBatRecordLimit=50;
// A settled, unloaded record for a Bat found on disk from an earlier run.
// lastAssistant is the transcript's final assistant row (or null).
Json savedBatRecord(const std::string& id,const std::string& description,
  const std::string& agentType,const std::string& poetry,std::int64_t startTime,
  std::int64_t endTime,std::size_t gadgetUses,const Json& lastAssistant,
  const std::string& outputFile);
SettledBatSweep sweepSettledBatMissions(const Json& missions,std::int64_t nowMs,
  std::size_t keepSettled=settledBatRecordLimit,
  const std::function<bool(const std::string&)>& busy={});
} // namespace batbox::bats
