#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace batbox::bootstrap {
struct SlowOperation {
  std::string operation;
  double durationMs=0;
  double timestamp=0;
};
using SlowOperationList=std::vector<SlowOperation>;
struct SlowOperationsState {
  std::shared_ptr<SlowOperationList> slowOperations=
    std::make_shared<SlowOperationList>();
};
using SlowClockMillis=std::function<double()>;
void addSlowOperation(SlowOperationsState& state,
  const std::string& operation,double durationMs,bool isDeveloper,
  const SlowClockMillis& clockMillis);
std::shared_ptr<const SlowOperationList> getSlowOperations(
  SlowOperationsState& state,const SlowClockMillis& clockMillis);
double indigenousSlowClockMillis();
}
