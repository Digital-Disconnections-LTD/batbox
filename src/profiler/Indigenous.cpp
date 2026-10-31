#include "profiler/Indigenous.hpp"
#include "utils/envUtils.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
#include <sstream>
#include <stdexcept>
#include <time.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#else
#include <unistd.h>
#endif

namespace batbox::profiler {
namespace {
std::optional<double> processStartMsSinceBoot(){
#if defined(__linux__)
  std::ifstream stat("/proc/self/stat");
  std::string line;std::getline(stat,line);
  const auto end=line.rfind(')');
  if(end==std::string::npos)return std::nullopt;
  std::istringstream fields(line.substr(end+2));
  std::string word;
  for(int field=3;field<=22;++field){
    if(!(fields>>word))return std::nullopt;
    if(field==22){
      const auto ticks=std::strtoull(word.c_str(),nullptr,10);
      const auto rate=sysconf(_SC_CLK_TCK);
      if(rate<=0)return std::nullopt;
      return static_cast<double>(ticks)*1000.0/rate;
    }
  }
  return std::nullopt;
#else
  return std::nullopt;
#endif
}
bool enabledEnv(const char* name){
  const auto* value=std::getenv(name);
  return isEnvTruthy(value?Json(value):Json());
}
}
double indigenousPerformanceNowMs(){
#if defined(__linux__)
  timespec clock{};
  if(clock_gettime(CLOCK_BOOTTIME,&clock)!=0)
    throw std::runtime_error("Cannot read monotonic process clock");
  const double now=clock.tv_sec*1000.0+clock.tv_nsec/1e6;
  static const auto start=processStartMsSinceBoot();
  if(start)return now-*start;
#endif
  static const auto fallback=std::chrono::steady_clock::now();
  return std::chrono::duration<double,std::milli>(
    std::chrono::steady_clock::now()-fallback).count();
}
MemoryUsage indigenousMemoryUsage(){
  MemoryUsage usage;
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS_EX info{};
  if(::GetProcessMemoryInfo(::GetCurrentProcess(),
      reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&info),sizeof(info)))
    usage.rss=static_cast<double>(info.WorkingSetSize);
#elif defined(__APPLE__)
  mach_task_basic_info_data_t info{};
  mach_msg_type_number_t count=MACH_TASK_BASIC_INFO_COUNT;
  if(::task_info(::mach_task_self(),MACH_TASK_BASIC_INFO,
      reinterpret_cast<task_info_t>(&info),&count)==KERN_SUCCESS)
    usage.rss=static_cast<double>(info.resident_size);
#else
  std::ifstream statm("/proc/self/statm");
  unsigned long pages=0,resident=0;
  if(statm>>pages>>resident){
    const auto pageSize=sysconf(_SC_PAGESIZE);
    if(pageSize>0)usage.rss=static_cast<double>(resident)*pageSize;
  }
#endif
#if defined(__GLIBC__)
  usage.heapUsed=static_cast<double>(mallinfo2().uordblks);
#endif
  return usage;
}
std::shared_ptr<Timeline> makeIndigenousTimeline(){
  return std::make_shared<Timeline>(indigenousPerformanceNowMs);
}
QueryProfilerServices indigenousQueryProfilerServices(
    std::shared_ptr<Timeline> timeline,
    std::function<void(const std::string&)> localDebug){
  return {enabledEnv("BATBOX_PROFILE_QUERY"),std::move(timeline),
    indigenousMemoryUsage,std::move(localDebug)};
}
HeadlessProfilerServices indigenousHeadlessProfilerServices(
    std::shared_ptr<Timeline> timeline,
    std::function<bool()> isNonInteractiveFlight,
    std::function<void(const std::string&)> localDebug){
  return {enabledEnv("BATBOX_PROFILE_STARTUP"),std::move(timeline),
    std::move(isNonInteractiveFlight),[]{
      const auto* value=std::getenv("BATBOX_ENTRYPOINT");
      return value?std::string(value):std::string{};},std::move(localDebug)};
}
}
