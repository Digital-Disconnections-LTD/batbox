#include "shell/LoopMissionOutput.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::shell {
LoopMissionOutput::LoopMissionOutput(event_loop::Loop& loop,MissionOutputPoller& poller,
  std::string missionId,std::string path,ProgressCallback onProgress,
  std::function<void(std::exception_ptr)> onFailure):
  loop_(loop),poller_(poller),missionId_(std::move(missionId)),
  active_(std::make_shared<std::atomic_bool>(true)){
  if(!loop_.isOwnerThread())throw std::logic_error("TaskOutput owner thread required");
  if(missionId_.empty()||!onProgress||!onFailure)
    throw std::invalid_argument("TaskOutput task ID/progress/diagnostic required");
  const auto dispatcher=loop_.dispatcher();
  const auto active=active_;
  poller_.registerOutput(missionId_,std::move(path),
    [dispatcher,active,onProgress=std::move(onProgress),onFailure=std::move(onFailure)](
      const PollSample& sample){
      if(!active->load(std::memory_order_acquire))return;
      try{dispatcher.postMission([active,onProgress,onFailure,sample]{
        if(!active->load(std::memory_order_acquire))return;
        try{onProgress(sample);}catch(...){onFailure(std::current_exception());}
      });}catch(const std::logic_error&){}
    });
}
LoopMissionOutput::~LoopMissionOutput(){stop();}
void LoopMissionOutput::start(){
  if(!loop_.isOwnerThread())throw std::logic_error("TaskOutput owner thread required");
  if(stopped_)throw std::logic_error("stopped TaskOutput registration cannot restart");
  if(started_)return;
  started_=true;poller_.startPolling(missionId_);
}
void LoopMissionOutput::stop(){
  if(stopped_)return;
  stopped_=true;
  active_->store(false,std::memory_order_release);
  if(started_)poller_.stopPolling(missionId_);
  poller_.unregisterOutput(missionId_);
  started_=false;
}
} // namespace batbox::shell
