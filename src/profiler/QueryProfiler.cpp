#include "profiler/QueryProfiler.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace batbox::profiler {
namespace {
std::string repeat(const std::string& unit,std::size_t count){
  std::string result;for(std::size_t i=0;i<count;++i)result+=unit;return result;
}
std::string join(const std::vector<std::string>& lines){
  std::string result;
  for(std::size_t i=0;i<lines.size();++i){if(i)result+='\n';result+=lines[i];}
  return result;
}
std::string padEnd(std::string text,std::size_t width){
  if(text.size()<width)text.append(width-text.size(),' ');
  return text;
}
std::string padStart(std::string text,std::size_t width){
  if(text.size()<width)text.insert(text.begin(),width-text.size(),' ');
  return text;
}
}
QueryProfiler::QueryProfiler(QueryProfilerServices services):services_(std::move(services)){
  if(!services_.timeline||!services_.memoryUsage||!services_.logForDebugging)
    throw std::invalid_argument("Query profiler imports are required");
}
void QueryProfiler::startQueryProfile(){
  if(!services_.enabled)return;
  services_.timeline->clearMarks();
  memorySnapshots_.clear();
  firstTokenTime_.reset();
  ++queryCount_;
  queryCheckpoint("query_user_input_received");
}
void QueryProfiler::queryCheckpoint(const std::string& name){
  if(!services_.enabled)return;
  services_.timeline->mark(name);
  memorySnapshots_[name]=services_.memoryUsage();
  if(name=="query_first_chunk_received"&&!firstTokenTime_){
    const auto& marks=services_.timeline->marks();
    if(!marks.empty())firstTokenTime_=marks.back().startTime;
  }
}
void QueryProfiler::endQueryProfile(){
  if(!services_.enabled)return;
  queryCheckpoint("query_profile_end");
}
std::string QueryProfiler::getSlowWarning(double delta,const std::string& name){
  if(name=="query_user_input_received")return "";
  if(delta>1000)return " ⚠️  VERY SLOW";
  if(delta>100)return " ⚠️  SLOW";
  if(name.find("git_status")!=std::string::npos&&delta>50)
    return " ⚠️  git status";
  if(name.find("tool_schema")!=std::string::npos&&delta>50)
    return " ⚠️  tool schemas";
  if(name.find("client_creation")!=std::string::npos&&delta>50)
    return " ⚠️  client creation";
  return "";
}
std::string QueryProfiler::getQueryProfileReport()const{
  if(!services_.enabled)
    return "Query profiling not enabled (set BATBOX_PROFILE_QUERY=1)";
  const auto& marks=services_.timeline->marks();
  if(marks.empty())return "No query profiling checkpoints recorded";
  std::vector<std::string> lines;
  lines.push_back(repeat("=",80));
  lines.push_back("QUERY PROFILING REPORT - Query #"+std::to_string(queryCount_));
  lines.push_back(repeat("=",80));
  lines.push_back("");
  const double baseline=marks.front().startTime;
  double previous=baseline,requestTime=0,firstChunk=0;
  for(const auto& mark:marks){
    const double relative=mark.startTime-baseline;
    const double delta=mark.startTime-previous;
    const auto memory=memorySnapshots_.find(mark.name);
    lines.push_back(formatTimelineLine(relative,delta,mark.name,
      memory==memorySnapshots_.end()?std::nullopt:
        std::optional(memory->second),10,9,getSlowWarning(delta,mark.name)));
    if(mark.name=="query_api_request_sent")requestTime=relative;
    if(mark.name=="query_first_chunk_received")firstChunk=relative;
    previous=mark.startTime;
  }
  const double total=marks.back().startTime-baseline;
  lines.push_back("");lines.push_back(repeat("-",80));
  if(firstChunk>0){
    const auto overhead=requestTime,latency=firstChunk-requestTime;
    const auto percent=[](double part,double total){
      std::ostringstream out;
      out<<std::fixed<<std::setprecision(1)<<(part/total)*100;
      return out.str();
    };
    lines.push_back("Total TTFT: "+formatMs(firstChunk)+"ms");
    lines.push_back("  - Pre-request overhead: "+formatMs(overhead)+
      "ms ("+percent(overhead,firstChunk)+"%)");
    lines.push_back("  - Network latency: "+formatMs(latency)+
      "ms ("+percent(latency,firstChunk)+"%)");
  }else lines.push_back("Total time: "+formatMs(total)+"ms");
  lines.push_back(getPhaseSummary(marks,baseline));
  lines.push_back(repeat("=",80));
  return join(lines);
}
std::string QueryProfiler::getPhaseSummary(const std::vector<Mark>& marks,
    double baseline)const{
  struct Phase{const char* name;const char* start;const char* end;};
  static const Phase phases[]={
    {"Context loading","query_context_loading_start","query_context_loading_end"},
    {"Microcompact","query_microcompact_start","query_microcompact_end"},
    {"Autocompact","query_autocompact_start","query_autocompact_end"},
    {"Query setup","query_setup_start","query_setup_end"},
    {"Tool schemas","query_tool_schema_build_start","query_tool_schema_build_end"},
    {"Message normalization","query_message_normalization_start","query_message_normalization_end"},
    {"Client creation","query_client_creation_start","query_client_creation_end"},
    {"Network TTFB","query_api_request_sent","query_first_chunk_received"},
    {"Tool execution","query_tool_execution_start","query_tool_execution_end"}};
  std::map<std::string,double> times;
  for(const auto& mark:marks)times[mark.name]=mark.startTime-baseline;
  std::vector<std::string> lines{"","PHASE BREAKDOWN:"};
  for(const auto& phase:phases){
    if(!times.contains(phase.start)||!times.contains(phase.end))continue;
    const auto duration=times.at(phase.end)-times.at(phase.start);
    const auto blocks=std::min(50.0,std::ceil(duration/10));
    if(blocks<0)throw std::range_error("Invalid count value");
    lines.push_back("  "+padEnd(phase.name,22)+" "+
      padStart(formatMs(duration),10)+"ms "+
      repeat("█",static_cast<std::size_t>(blocks)));
  }
  if(times.contains("query_api_request_sent")){
    lines.push_back("");
    lines.push_back("  "+padEnd("Total pre-API overhead",22)+" "+
      padStart(formatMs(times.at("query_api_request_sent")),10)+"ms");
  }
  return join(lines);
}
void QueryProfiler::logQueryProfileReport()const{
  if(!services_.enabled)return;
  services_.logForDebugging(getQueryProfileReport());
}
}
