#include "model/StreamObservation.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace batbox::model {
namespace {
std::string oneDecimal(double value){
  // Number.toFixed rounds the exact binary64 value at the decimal midpoint;
  // iostream's ties-to-even rule differs for e.g. 190.25 -> 190.3 in JS.
  const auto tenths=static_cast<std::int64_t>(std::floor(
    static_cast<long double>(value)*10.0L+0.5L));
  return std::to_string(tenths/10)+"."+std::to_string(tenths%10);
}
std::int64_t nowMillis(){
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}
}
StreamObservation::StreamObservation(std::function<std::int64_t()> now,
    std::function<void(const std::string&,const std::string&)> debug,
    std::function<void(const std::string&,const std::string&)> diagnostic):
  nowMillis_(now?std::move(now):nowMillis),debug_(std::move(debug)),
  diagnostic_(std::move(diagnostic)){}
void StreamObservation::onEvent(){
  const auto now=nowMillis_();
  if(lastEventTime_){
    const auto gap=now-*lastEventTime_;
    if(gap>30000){
      ++stallCount_;
      totalStallTimeMs_+=gap;
      if(debug_)debug_("Streaming stall detected: "+
        oneDecimal(gap/1000.0)+"s gap between events (stall #"+
        std::to_string(stallCount_)+")","warn");
    }
  }
  lastEventTime_=now;
}
void StreamObservation::beforeFinish(const streaming::Reducer& reducer){
  beforeFinish(reducer.hasMessageStart(),reducer.messages().size(),
    value::truthy(reducer.stopReason()));
}
void StreamObservation::beforeFinish(bool hasMessageStart,
    std::size_t completedMessages,bool hasStopReason){
  if(!hasMessageStart||(completedMessages==0&&!hasStopReason)){
    if(debug_)debug_(hasMessageStart
      ?"Stream completed with message_start but no content blocks completed - triggering non-streaming fallback"
      :"Stream completed without receiving message_start event - triggering non-streaming fallback",
      "error");
    return;
  }
  if(stallCount_>0&&debug_)debug_("Streaming completed with "+
    std::to_string(stallCount_)+" stall(s), total stall time: "+
    oneDecimal(totalStallTimeMs_/1000.0)+"s","warn");
}
void StreamObservation::watchdogCleanExit(){
  watchdogCleanReported_=true;
  if(diagnostic_)diagnostic_("info","cli_stream_loop_exited_after_watchdog_clean");
}
void StreamObservation::watchdogErrorExit(){
  if(!watchdogCleanReported_&&diagnostic_)
    diagnostic_("info","cli_stream_loop_exited_after_watchdog_error");
}
}
