#include "memory_prefetch/Prefetch.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace batbox;
namespace {
Json run(const Json& input){
  event_loop::Loop loop;
  instructions::MemoryPathStash pathStash;
  memory_prefetch::Services services{loop};
  services.pathStash=&pathStash;
  services.paths.projectRoot="/tmp";
  services.paths.environment={{services.paths.keys.fullPathOverride,
    "/tmp/batbox-memory-prefetch-fixture-empty"}};
  if(input.value("autoEnabled",true)==false)
    services.paths.environment[services.paths.keys.disableAutoMemory]="1";
  int featureCalls=0;
  services.searchFeatureEnabled=[&]{++featureCalls;return input.value("feature",true);};
  services.logError=[](std::exception_ptr){};
  services.isAbortError=[](const std::exception&){return false;};
  file_state_stash::FileStateStash stash(100);
  execution::Context context;
  context.readFileState=std::shared_ptr<file_state_stash::FileStateStash>(&stash,
    [](auto*){});
  context.data={{"options",{{"agentDefinitions",{{"activeAgents",Json::array()}}}}}};
  const auto& messages=value::property(input,"messages");
  auto handle=memory_prefetch::startRelevantMemoryPrefetch(
    messages.is_array()?messages:Json::array(),context,services);
  if(!handle.isSettled)return {{"started",false},{"featureCalls",featureCalls}};
  const bool initiallySettled=handle.isSettled();
  for(int i=0;i<2000&&!handle.isSettled();++i){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  loop.runReady();
  if(!handle.isSettled())throw std::runtime_error("Prefetch did not settle");
  return {{"started",true},{"initiallySettled",initiallySettled},
    {"settled",handle.isSettled()},{"result",handle.read()},
    {"featureCalls",featureCalls}};
}
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
