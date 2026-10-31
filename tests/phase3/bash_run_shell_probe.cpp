#include <cstdlib>
#include "gadgets/bash/RunShellCommand.hpp"
#include "runtime/JsonValue.hpp"
#include <atomic>
#include <filesystem>
#include <iostream>
#include <unistd.h>
int main(){
  char temp[]="/tmp/batbox-run-shell-probe-XXXXXX";if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=temp;
  try{
    std::atomic<int> ids{0};
    batbox::shell::FlightConfig config;config.shellPath="/bin/bash";config.originalCwd=root.string();
    config.outputDir=(root/"jobs").string();config.generateMissionId=[&]{return "task"+std::to_string(++ids);};
    config.splitCommand=[](const std::string&){return std::vector<std::string>{"echo"};};
    batbox::shell::MissionOutputPoller poller;batbox::Json output=batbox::Json::array();
    for(const std::string mode:{"quick","explicit","slow"}){
      ids=0;
      auto shell=std::make_shared<batbox::shell::ManagedShell>(config);
      std::vector<std::string> events;
      batbox::Json progress=batbox::Json::array();
      batbox::bash_gadget::RunShellCallbacks cb;
      cb.spawnBackgroundMission=[&](const batbox::shell::CommandHandle& handle){
        events.push_back("spawn");handle.background(handle.missionId());
        events.push_back("background:"+handle.missionId());return handle.missionId();};
      cb.registerForeground=[](const auto&)->std::string{throw std::runtime_error("unexpected foreground");};
      cb.backgroundExistingForegroundMission=[](const auto&,const auto&){return false;};
      cb.unregisterForeground=[](const auto&){};cb.markMissionNotified=[](const auto&){};
      cb.yieldProgress=[&](const auto& value){progress.push_back(value.toJson());};
      batbox::bash_gadget::RunShellSettings settings;settings.runInBackground=mode=="explicit";
      if(mode=="slow")settings.progressThresholdMs=20;
      auto result=batbox::bash_gadget::runShellCommand(shell,poller,
        batbox::shell::RunOptions{.command=mode=="quick"?"echo quick":
          mode=="slow"?"echo hello; sleep 1.3":"sleep 0.2",
          .timeoutMs=5000},settings,cb);
      output.push_back({{"value",result.toJson()},{"events",events},{"progress",progress}});
      if(result.backgroundMissionId){shell->wait(*result.backgroundMissionId,2000);
        shell->cleanup(*result.backgroundMissionId);}
    }
    std::cout<<batbox::value::stringify(output)<<'\n';
    std::filesystem::remove_all(root);return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';
    std::filesystem::remove_all(root);return 1;}
}
