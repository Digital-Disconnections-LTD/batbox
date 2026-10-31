#include "profiler/Indigenous.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>

int main(){
  setenv("BATBOX_PROFILE_QUERY","1",1);
  setenv("BATBOX_PROFILE_STARTUP","1",1);
  setenv("BATBOX_ENTRYPOINT","sdk-cli",1);
  auto timeline=batbox::profiler::makeIndigenousTimeline();
  const auto start=timeline->now();
  auto memory=batbox::profiler::indigenousMemoryUsage();
  const auto end=timeline->now();
  assert(start>=0&&end>=start);
  assert(memory.rss>0);
  int logs=0;
  auto query=batbox::profiler::QueryProfiler(
    batbox::profiler::indigenousQueryProfilerServices(timeline,
      [&](const std::string&){++logs;}));
  auto headless=batbox::profiler::HeadlessProfiler(
    batbox::profiler::indigenousHeadlessProfilerServices(timeline,
      []{return true;},[&](const std::string&){++logs;}));
  query.startQueryProfile();
  headless.headlessProfilerStartTurn();
  headless.headlessProfilerCheckpoint("first_chunk");
  headless.logHeadlessProfilerTurn();
  query.logQueryProfileReport();
  assert(logs==4);
  std::cout<<"native monotonic clock, memory and profile env bindings pass\n";
}
