#pragma once
#include "shell/ManagedShell.hpp"
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <stdexcept>
#include <string>
#include <vector>

namespace batbox::shell_missions {
struct MissionRecord {
  std::string id,type="local_bash",status="running",description,command;
  std::optional<std::string> gadgetUseId,batId,kind,poetry,result,error;
  std::optional<int> exitCode;
  bool interrupted=false,notified=false,isBackgrounded=true;
  long long startTimeMs=0;
  std::optional<long long> endTimeMs;
  std::optional<std::string> outputFilePath;
};
struct StopResult {std::string missionId,missionType;std::optional<std::string> command;};
class StopMissionError final:public std::runtime_error {
public:
  StopMissionError(std::string message,std::string code);
  std::string code;
};
class MissionAbortError final:public std::runtime_error {
public:
  MissionAbortError():std::runtime_error(""){}
};
struct Services {
  std::function<long long()> nowMs;
  std::function<void(const std::string&,const std::optional<std::string>&)> enqueueNotification;
  std::function<void(const MissionRecord&)> onMissionTerminated;
  std::function<void(const MissionRecord&)> stopExternalMission;
  std::function<std::string(const MissionRecord&)> readExternalOutput;
  std::function<void(const MissionRecord&)> markExternalNotified;
};
class Roster {
public:
  explicit Roster(Services services);
  void attachShell(std::shared_ptr<shell::ManagedShell> shell);
  void registerShell(MissionRecord record);
  void adoptBackground(MissionRecord record);
  void registerForeground(MissionRecord record);
  bool backgroundExistingForeground(const std::string& missionId);
  bool unregisterForeground(const std::string& missionId);
  std::vector<std::string> foregroundShellIds()const;
  std::vector<std::string> backgroundAllForeground();
  void registerExternal(MissionRecord record);
  void upsertExternal(MissionRecord record);
  void eraseExternal(const std::string& missionId);
  void bindExternalServices(std::function<void(const MissionRecord&)> stop,
    std::function<std::string(const MissionRecord&)> read,
    std::function<void(const MissionRecord&)> markNotified);
  void completeShell(const std::string& missionId,const shell::ExecResult& result);
  std::optional<MissionRecord> get(const std::string& missionId)const;
  std::vector<MissionRecord> list()const;
  std::optional<MissionRecord> wait(const std::string& missionId,double timeoutMs,std::stop_token signal);
  bool markNotified(const std::string& missionId);
  StopResult stop(const std::string& missionId);
  void stopForBat(const std::string& batId);
  std::string readOutput(const MissionRecord& mission)const;
private:
  Services services_;
  std::shared_ptr<shell::ManagedShell> shell_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::map<std::string,MissionRecord> missions_;
  std::map<std::string,shell::ExecResult> earlyCompletions_;
  void completeLocked(MissionRecord& mission,const shell::ExecResult& result,
    std::optional<std::string>& notification);
};
std::string escapeXml(const std::string& text);
} // namespace batbox::shell_missions
