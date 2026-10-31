#include "profiler/HeadlessProfiler.hpp"
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

namespace batbox::profiler {
namespace {
constexpr const char* prefix="headless_";
std::string oneDecimal(double value){
  std::ostringstream out;out<<std::fixed<<std::setprecision(1)<<value;
  return out.str();
}
}
HeadlessProfiler::HeadlessProfiler(HeadlessProfilerServices services)
    :services_(std::move(services)){
  if(!services_.timeline||!services_.isNonInteractiveFlight||
      !services_.entrypoint||!services_.logForDebugging)
    throw std::invalid_argument("Headless profiler imports are required");
}
void HeadlessProfiler::clearHeadlessMarks(){
  const auto all=services_.timeline->marks();
  for(const auto& mark:all)
    if(mark.name.starts_with(prefix))services_.timeline->clearMarks(mark.name);
}
void HeadlessProfiler::headlessProfilerStartTurn(){
  if(!services_.isNonInteractiveFlight())return;
  if(!services_.detailed)return;
  ++currentTurnNumber_;
  clearHeadlessMarks();
  services_.timeline->mark(std::string(prefix)+"turn_start");
  services_.logForDebugging("[headlessProfiler] Started turn "+
    std::to_string(currentTurnNumber_));
}
void HeadlessProfiler::headlessProfilerCheckpoint(const std::string& name){
  if(!services_.isNonInteractiveFlight())return;
  if(!services_.detailed)return;
  services_.timeline->mark(std::string(prefix)+name);
  services_.logForDebugging("[headlessProfiler] Checkpoint: "+name+
    " at "+oneDecimal(services_.timeline->now())+"ms");
}
void HeadlessProfiler::logHeadlessProfilerTurn(){
  if(!services_.isNonInteractiveFlight())return;
  if(!services_.detailed)return;
  const auto& all=services_.timeline->marks();
  std::vector<Mark> marks;
  for(const auto& mark:all)if(mark.name.starts_with(prefix))marks.push_back(mark);
  if(marks.empty())return;
  std::map<std::string,double> times;
  for(const auto& mark:marks)
    times[mark.name.substr(std::char_traits<char>::length(prefix))]=mark.startTime;
  if(!times.contains("turn_start"))return;
  const auto turnStart=times.at("turn_start");
  Json metadata={{"turn_number",currentTurnNumber_}};
  if(times.contains("system_message_yielded")&&currentTurnNumber_==0)
    metadata["time_to_system_message_ms"]=
      static_cast<long long>(std::floor(times.at("system_message_yielded")+0.5));
  if(times.contains("query_started"))
    metadata["time_to_query_start_ms"]=
      static_cast<long long>(std::floor(times.at("query_started")-turnStart+0.5));
  if(times.contains("first_chunk"))
    metadata["time_to_first_response_ms"]=
      static_cast<long long>(std::floor(times.at("first_chunk")-turnStart+0.5));
  if(times.contains("query_started")&&times.contains("api_request_sent"))
    metadata["query_overhead_ms"]=
      static_cast<long long>(std::floor(times.at("api_request_sent")-
        times.at("query_started")+0.5));
  metadata["checkpoint_count"]=marks.size();
  const auto entrypoint=services_.entrypoint();
  if(!entrypoint.empty())metadata["entrypoint"]=entrypoint;
  // Keep detailed profiling output local; do not upload sampled analytics.
  services_.logForDebugging("[headlessProfiler] Turn "+
    std::to_string(currentTurnNumber_)+" metrics: "+value::stringify(metadata));
}
}
