#include "profiler/ModelBindings.hpp"
#include <iostream>

int main(){
  using batbox::Json;
  const auto input=Json::parse(std::cin);
  double now=input.value("now",0.0);
  batbox::profiler::MemoryUsage memory{2048,1024};
  bool interactive=false;
  Json logs=Json::array(),out=Json::array();
  auto timeline=std::make_shared<batbox::profiler::Timeline>([&]{return now;});
  batbox::profiler::QueryProfiler query({input.value("queryEnabled",false),
    timeline,[&]{return memory;},[&](const std::string& text){logs.push_back(text);}});
  batbox::profiler::HeadlessProfiler headless({input.value("detailed",false),
    timeline,[&]{return !interactive;},
    [&]{return input.value("entrypoint",std::string{});},
    [&](const std::string& text){logs.push_back(text);}});
  for(const auto& action:input["actions"]){
    const auto kind=action["kind"].get<std::string>();
    Json actionError=nullptr;
    try{
    if(kind=="time")now=action["value"].get<double>();
    if(kind=="memory"){
      memory.rss=action["value"]["rss"].get<double>();
      memory.heapUsed=action["value"]["heapUsed"].get<double>();
    }
    if(kind=="interactive")interactive=action["value"].get<bool>();
    if(kind=="queryStart")query.startQueryProfile();
    if(kind=="queryMark")query.queryCheckpoint(action["name"].get<std::string>());
    if(kind=="queryEnd")query.endQueryProfile();
    if(kind=="queryLog")query.logQueryProfileReport();
    if(kind=="headlessStart")headless.headlessProfilerStartTurn();
    if(kind=="headlessMark")headless.headlessProfilerCheckpoint(
      action["name"].get<std::string>());
    if(kind=="headlessLog")headless.logHeadlessProfilerTurn();
    }catch(const std::exception& error){actionError=error.what();}
    Json marks=Json::array();
    for(const auto& mark:timeline->marks())
      marks.push_back({{"name",mark.name},{"startTime",mark.startTime}});
    Json report;
    try{report=query.getQueryProfileReport();}
    catch(const std::exception& error){report={{"error",error.what()}};}
    out.push_back({{"marks",marks},{"logs",logs},{"remote",Json::array()},
      {"queryCount",query.queryCount()},
      {"firstTokenTime",query.firstTokenTime()?
        Json(*query.firstTokenTime()):Json(nullptr)},
      {"currentTurnNumber",headless.currentTurnNumber()},
      {"report",report},{"actionError",actionError}});
  }
  std::cout<<out.dump()<<'\n';
}
