#include "profiler/Indigenous.hpp"
#include <fstream>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <malloc/malloc.h>
#else
#include <unistd.h>
#endif

namespace batbox::profiler {
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
  // Blocks of 128 KiB or more are mmapped by glibc and counted in hblkhd.
  const auto info=mallinfo2();
  usage.heapUsed=static_cast<double>(info.uordblks+info.hblkhd);
#elif defined(__APPLE__)
  malloc_statistics_t heap{};
  ::malloc_zone_statistics(nullptr,&heap);
  usage.heapUsed=static_cast<double>(heap.size_in_use);
#endif
  return usage;
}
}
