#pragma once
#include "streaming/Reducer.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace batbox::model {
// Track stream progress and stalls locally within the queryModel loop.
class StreamObservation {
public:
  StreamObservation(std::function<std::int64_t()> nowMillis,
    std::function<void(const std::string&,const std::string&)> debug,
    std::function<void(const std::string&,const std::string&)> diagnostic={});
  void onEvent();
  // Called after the for-await loop and before reducer.finish().
  void beforeFinish(const streaming::Reducer& reducer);
  void beforeFinish(bool hasMessageStart,std::size_t completedMessages,bool hasStopReason);
  void watchdogCleanExit();
  void watchdogErrorExit();
  int stallCount()const{return stallCount_;}
  std::int64_t totalStallTimeMs()const{return totalStallTimeMs_;}
private:
  std::function<std::int64_t()> nowMillis_;
  std::function<void(const std::string&,const std::string&)> debug_;
  std::function<void(const std::string&,const std::string&)> diagnostic_;
  std::optional<std::int64_t> lastEventTime_;
  std::int64_t totalStallTimeMs_=0;
  int stallCount_=0;
  bool watchdogCleanReported_=false;
};
}
