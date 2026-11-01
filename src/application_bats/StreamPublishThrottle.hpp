#pragma once
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>

namespace batbox::application_bats {
// Decides when an observed Bat's live stream projection is copied into
// AppState. Each copy snapshots the whole state, so stream deltas publish at
// most once per interval; complete messages publish at once. A delta inside
// the interval schedules one trailing publish so a burst's final text lands.
class StreamPublishThrottle {
public:
  enum class Decision {Now,Schedule,Pending};
  struct Step {Decision decision=Decision::Now;std::int64_t delayMs=0;};
  explicit StreamPublishThrottle(std::int64_t intervalMs=100):interval_(intervalMs){}
  Step onEvent(const std::string& id,std::int64_t nowMs,bool completeMessage){
    auto& entry=entries_[id];
    if(completeMessage)return {Decision::Now,0};
    if(entry.scheduled)return {Decision::Pending,0};
    if(nowMs-entry.lastMs>=interval_)return {Decision::Now,0};
    entry.scheduled=true;
    return {Decision::Schedule,entry.lastMs+interval_-nowMs};
  }
  void published(const std::string& id,std::int64_t nowMs){entries_[id].lastMs=nowMs;}
  void timerFired(const std::string& id){
    if(const auto at=entries_.find(id);at!=entries_.end())at->second.scheduled=false;
  }
  void forget(const std::string& id){entries_.erase(id);}
  std::int64_t intervalMs()const{return interval_;}
private:
  struct Entry {
    std::int64_t lastMs=std::numeric_limits<std::int64_t>::min()/2;
    bool scheduled=false;
  };
  std::unordered_map<std::string,Entry> entries_;
  std::int64_t interval_;
};
} // namespace batbox::application_bats
