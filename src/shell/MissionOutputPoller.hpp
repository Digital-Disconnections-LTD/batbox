#pragma once
#include "shell/TailFile.hpp"
#include <cstddef>
#include <condition_variable>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace batbox::shell {
struct PollSample {
  std::string lastLines,allLines;
  std::size_t totalLines=0,totalBytes=0;
  bool isIncomplete=false;
};
// Calculate file-mode MissionOutput content and line counts. The caller
// preserves totalLines between ticks and controls the polling schedule.
PollSample sampleMissionOutputTail(const TailResult& tail,std::size_t previousTotalLines);

// Indigenous scheduler for MissionOutput's shared file-mode poll interval. Callbacks
// run after the file read, outside the roster lock. This owner explicitly
// stops and joins its Indigenous worker.
class MissionOutputPoller {
public:
  using Callback=std::function<void(const PollSample&)>;
  MissionOutputPoller()=default;
  ~MissionOutputPoller();
  MissionOutputPoller(const MissionOutputPoller&)=delete;
  MissionOutputPoller& operator=(const MissionOutputPoller&)=delete;
  void registerOutput(std::string missionId,std::string path,Callback callback);
  void startPolling(const std::string& missionId);
  void stopPolling(const std::string& missionId);
  void unregisterOutput(const std::string& missionId);
  void shutdown();
  std::exception_ptr takeCallbackFailure(const std::string& missionId);
  void tick();
private:
  struct Entry {std::string path;Callback callback;std::size_t totalLines=0;
    bool active=false;std::exception_ptr callbackFailure;};
  std::map<std::string,std::shared_ptr<Entry>> entries_;
  std::vector<std::string> activeOrder_;
  std::mutex mutex_;
  std::condition_variable_any cv_;
  std::jthread worker_;
  bool active_=false;
  bool closed_=false;
  void run(std::stop_token token);
};
}
