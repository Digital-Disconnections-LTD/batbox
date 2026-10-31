#include "bootstrap/SlowOperations.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::bootstrap {
namespace {
constexpr double slowOperationTtlMs=10000;
constexpr std::size_t maxSlowOperations=10;
const std::shared_ptr<const SlowOperationList> emptySlowOperations=
  std::make_shared<const SlowOperationList>();
void needClock(const SlowClockMillis& clockMillis){
  if(!clockMillis)throw std::invalid_argument("clockMillis is required");
}
}
void addSlowOperation(SlowOperationsState& state,
  const std::string& operation,double durationMs,bool isDeveloper,
  const SlowClockMillis& clockMillis){
  if(!isDeveloper)return;
  if(operation.find("exec")!=std::string::npos&&
     operation.find("batbox-prompt-")!=std::string::npos)return;
  needClock(clockMillis);
  const double now=clockMillis();
  auto fresh=std::make_shared<SlowOperationList>();
  for(const auto& item:*state.slowOperations)
    if(now-item.timestamp<slowOperationTtlMs)fresh->push_back(item);
  state.slowOperations=std::move(fresh);
  state.slowOperations->push_back({operation,durationMs,now});
  if(state.slowOperations->size()>maxSlowOperations){
    auto recent=std::make_shared<SlowOperationList>(
      state.slowOperations->end()-maxSlowOperations,
      state.slowOperations->end());
    state.slowOperations=std::move(recent);
  }
}
std::shared_ptr<const SlowOperationList> getSlowOperations(
  SlowOperationsState& state,const SlowClockMillis& clockMillis){
  if(state.slowOperations->empty())return emptySlowOperations;
  needClock(clockMillis);
  const double now=clockMillis();
  const auto expired=std::any_of(state.slowOperations->begin(),
    state.slowOperations->end(),[&](const auto& item){
      return now-item.timestamp>=slowOperationTtlMs;
    });
  if(expired){
    auto fresh=std::make_shared<SlowOperationList>();
    for(const auto& item:*state.slowOperations)
      if(now-item.timestamp<slowOperationTtlMs)fresh->push_back(item);
    state.slowOperations=std::move(fresh);
    if(state.slowOperations->empty())return emptySlowOperations;
  }
  return state.slowOperations;
}
}
