#pragma once
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace batbox::shell_provider {class BashProvider;}
namespace batbox::shell {
struct CwdCompletionServices;
enum class CommandStatus { Running, Backgrounded, Completed, Killed };
struct CommandStartSignal {
  std::mutex mutex;
  std::condition_variable cv;
  bool started=false;
  bool finished=false;
};
struct ExecResult {
  std::string stdoutText;
  std::string stderrText;
  int code=0;
  bool interrupted=false;
  std::optional<std::string> backgroundMissionId;
  bool backgroundedByUser=false;
  bool assistantAutoBackgrounded=false;
  // Background results include this property even when false.
  // Other result constructors leave it absent.
  bool assistantAutoBackgroundedOwnProperty=false;
  std::optional<std::string> outputFilePath;
  std::optional<std::size_t> outputFileSize;
  std::optional<std::string> outputMissionId;
  std::optional<std::string> preSpawnError;
  Json toJson()const;
};
struct Progress {
  std::string lastLines;
  std::string fullOutput;
  double elapsedTimeSeconds=0;
  std::size_t totalLines=0;
  std::size_t totalBytes=0;
  std::string missionId;
  std::optional<double> timeoutMs;
  Json toJson()const;
};
struct FlightConfig {
  std::string shellPath;
  std::string originalCwd;
  std::string outputDir;
  std::map<std::string,std::string> environmentOverrides;
  std::size_t maxInlineOutputBytes=30000;
  std::size_t maxBackgroundOutputBytes=5ull*1024*1024*1024;
  double defaultTimeoutMs=120000;
  double maxTimeoutMs=600000;
  bool backgroundMissionsDisabled=false;
  bool assistantAutoBackgroundEnabled=false;
  double assistantBlockingBudgetMs=15000;
  std::function<std::string()> generateMissionId;
  std::function<std::string()> generateExecId;
  std::shared_ptr<shell_provider::BashProvider> provider;
  std::function<std::vector<std::string>(const std::string&)> splitCommand;
  std::function<void(const std::string&,const ExecResult&)> onBackgroundComplete;
  std::function<void(const std::string&,std::exception_ptr)> onBackgroundError;
  std::shared_ptr<CwdCompletionServices> cwdCompletion;
};
struct RunOptions {
  std::string command;
  std::string description;
  std::optional<double> timeoutMs;
  bool runInBackground=false;
  bool preventCwdChanges=false;
  bool isMainThread=true;
  bool autoBackgroundEnabled=true;
  bool managedAssistantBackgroundEnabled=true;
  bool useSandbox=false;
  std::optional<std::string> sandboxTmpDir;
  std::function<std::string(const std::string&)> sandboxWrap;
  std::function<void(const std::string&)> logDebug;
  std::stop_token signal;
  std::function<std::string()> currentAbortReason;
  std::function<void(const Progress&)> onProgress;
  std::optional<std::string> reservedMissionId;
  std::optional<std::string> preparedCommandString;
  std::optional<std::string> preparedCwdFilePath;
  std::function<bool(const std::function<bool(const std::string&)>&)> onTimeout;
  std::shared_ptr<CommandStartSignal> startSignal;
};
class ManagedShell {
public:
  struct Job;
  explicit ManagedShell(FlightConfig config);
  ExecResult run(const RunOptions& options);
  ExecResult wait(const std::string& missionId,double maxWaitMs);
  ExecResult stop(const std::string& missionId);
  bool requestKill(const std::string& missionId);
  bool background(const std::string& missionId,bool byUser=true);
  bool cleanup(const std::string& missionId);
  bool release(const std::string& missionId); // ShellCommand.cleanup keeps large output on disk
  void beginShutdown();
  std::vector<std::string> activeJobIds()const;
  void waitForShutdown(const std::string& missionId);
  std::string allocateMissionId()const;
  std::optional<CommandStatus> status(const std::string& missionId)const;
  std::string cwd()const;
  void setCwd(const std::string& path);
  std::string outputPath(const std::string& missionId)const;
  std::string readOutput(const std::string& missionId)const;
  std::exception_ptr backgroundCallbackFailure(const std::string& missionId)const;
private:
  FlightConfig config_;
  mutable std::mutex mutex_;
  std::string cwd_;
  std::map<std::string,std::shared_ptr<Job>> jobs_;
  bool shuttingDown_=false;
  std::shared_ptr<Job> spawn(const RunOptions& options,const std::string& missionId,
    const std::string& cwd);
  ExecResult resultFor(const std::shared_ptr<Job>& job,bool background);
  void updateCwd(const std::shared_ptr<Job>& job,const RunOptions& options,
    const ExecResult& result);
};
std::string findSuitableShell(const std::map<std::string,std::string>& env);
bool isAutobackgroundingAllowed(const std::string& command,
  const std::function<std::vector<std::string>(const std::string&)>& splitCommand);
} // namespace batbox::shell
