#pragma once
#include "profiler/Timeline.hpp"
#include <map>
#include <memory>

namespace batbox::profiler {
struct QueryProfilerServices {
  bool enabled=false;
  std::shared_ptr<Timeline> timeline;
  std::function<MemoryUsage()> memoryUsage;
  std::function<void(const std::string&)> logForDebugging;
};

class QueryProfiler {
public:
  explicit QueryProfiler(QueryProfilerServices services);
  void startQueryProfile();
  void queryCheckpoint(const std::string& name);
  void endQueryProfile();
  std::string getQueryProfileReport() const;
  void logQueryProfileReport() const;
  int queryCount() const{return queryCount_;}
  std::optional<double> firstTokenTime() const{return firstTokenTime_;}
private:
  QueryProfilerServices services_;
  std::map<std::string,MemoryUsage> memorySnapshots_;
  int queryCount_=0;
  std::optional<double> firstTokenTime_;
  std::string getPhaseSummary(const std::vector<Mark>& marks,double baseline) const;
  static std::string getSlowWarning(double delta,const std::string& name);
};
}
