#pragma once
#include "profiler/Timeline.hpp"
#include "runtime/JsonValue.hpp"
#include <memory>

namespace batbox::profiler {
struct HeadlessProfilerServices {
  bool detailed=false;
  std::shared_ptr<Timeline> timeline;
  std::function<bool()> isNonInteractiveFlight;
  std::function<std::string()> entrypoint;
  std::function<void(const std::string&)> logForDebugging;
};
class HeadlessProfiler {
public:
  explicit HeadlessProfiler(HeadlessProfilerServices services);
  void headlessProfilerStartTurn();
  void headlessProfilerCheckpoint(const std::string& name);
  void logHeadlessProfilerTurn();
  int currentTurnNumber()const{return currentTurnNumber_;}
private:
  HeadlessProfilerServices services_;
  int currentTurnNumber_=-1;
  void clearHeadlessMarks();
};
}
