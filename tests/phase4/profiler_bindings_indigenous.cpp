#include "event_loop/Loop.hpp"
#include "profiler/ModelBindings.hpp"
#include <cassert>
#include <iostream>

int main(){
  batbox::event_loop::Loop loop;
  double now=0;
  auto timeline=std::make_shared<batbox::profiler::Timeline>([&]{
    assert(loop.isOwnerThread());return now;});
  std::vector<std::string> debug;
  auto query=std::make_shared<batbox::profiler::QueryProfiler>(
    batbox::profiler::QueryProfilerServices{true,timeline,
      []{return batbox::profiler::MemoryUsage{2048,1024};},
      [&](const std::string& line){debug.push_back(line);}});
  auto headless=std::make_shared<batbox::profiler::HeadlessProfiler>(
    batbox::profiler::HeadlessProfilerServices{true,timeline,
      []{return true;},[]{return std::string("sdk-cli");},
      [&](const std::string& line){debug.push_back(line);}});
  batbox::model::QueryServices services;
  batbox::profiler::bindModelProfiler(services,query,headless);
  loop.postMission([&]{
    query->startQueryProfile();
    headless->headlessProfilerStartTurn();
    now=50;
    services.queryCheckpoint("query_api_request_sent");
    services.headlessProfilerCheckpoint("api_request_sent");
    now=140;
    services.queryCheckpoint("query_first_chunk_received");
    services.headlessProfilerCheckpoint("first_chunk");
    services.endQueryProfile();
    headless->logHeadlessProfilerTurn();
    query->logQueryProfileReport();
  });
  loop.run();
  assert(query->firstTokenTime()==140);
  assert(query->queryCount()==1&&headless->currentTurnNumber()==0);
  assert(timeline->marks().size()==7);
  assert(debug.size()==5);
  assert(debug.back().find("QUERY PROFILING REPORT")!=std::string::npos);
  std::cout<<"shared owner profiler model bindings pass\n";
}
