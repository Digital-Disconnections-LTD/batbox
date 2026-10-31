#pragma once
#include "event_loop/Loop.hpp"
#include "shell/MissionOutputPoller.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace batbox::shell {
// Binds the shared indigenous file poller to the Node-style owner strand. Poller
// I/O remains on its worker; progress callbacks and their Promise wakeups run
// only after an owner mission. A stopped registration drops queued samples.
class LoopMissionOutput {
public:
  using ProgressCallback=std::function<void(const PollSample&)>;
  LoopMissionOutput(event_loop::Loop& loop,MissionOutputPoller& poller,
    std::string missionId,std::string path,ProgressCallback onProgress,
    std::function<void(std::exception_ptr)> onFailure);
  ~LoopMissionOutput();
  LoopMissionOutput(const LoopMissionOutput&)=delete;
  LoopMissionOutput& operator=(const LoopMissionOutput&)=delete;
  void start();
  void stop();
private:
  event_loop::Loop& loop_;
  MissionOutputPoller& poller_;
  std::string missionId_;
  std::shared_ptr<std::atomic_bool> active_;
  bool started_=false;
  bool stopped_=false;
};
} // namespace batbox::shell
