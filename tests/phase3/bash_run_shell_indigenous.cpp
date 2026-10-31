#include <cstdlib>
#include "gadgets/bash/RunShellCommand.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace batbox;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
  char temp[]="/tmp/batbox-run-shell-XXXXXX";if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=temp;
  try{
    std::atomic<int> ids{0};
    shell::FlightConfig config;config.shellPath="/bin/bash";config.originalCwd=root.string();
    config.outputDir=(root/"jobs").string();config.generateMissionId=[&]{return "task"+std::to_string(++ids);};
    config.splitCommand=[](const std::string&){return std::vector<std::string>{"echo"};};
    auto managed=std::make_shared<shell::ManagedShell>(config);
    shell::MissionOutputPoller poller;
    std::vector<std::string> events;
    auto callbacks=[&]{bash_gadget::RunShellCallbacks cb;
      cb.spawnBackgroundMission=[&](const shell::CommandHandle& handle){
        events.push_back("spawn:"+handle.missionId());
        check(handle.background(handle.missionId()),"background transition failed");
        return handle.missionId();};
      cb.registerForeground=[&](const shell::CommandHandle& handle){
        events.push_back("foreground:"+handle.missionId());return handle.missionId();};
      cb.backgroundExistingForegroundMission=[&](const std::string& id,const shell::CommandHandle& handle){
        events.push_back("adopt:"+id);return handle.background(id);};
      cb.unregisterForeground=[&](const std::string& id){events.push_back("unregister:"+id);};
      cb.markMissionNotified=[&](const std::string& id){events.push_back("notified:"+id);};
      cb.showBackgroundHint=[&]{events.push_back("hint");};
      cb.yieldProgress=[&](const shell::Progress& progress){
        events.push_back("progress:"+progress.missionId);
        check(progress.totalLines>=1,"polled line count missing");};
      return cb;};
    const auto quick=bash_gadget::runShellCommand(managed,poller,
      shell::RunOptions{.command="echo quick",.timeoutMs=5000},{},callbacks());
    check(quick.stdoutText.find("quick")!=std::string::npos&&events.empty(),
      "quick foreground should return before progress threshold");
    auto explicitOptions=bash_gadget::RunShellSettings{};explicitOptions.runInBackground=true;
    const auto explicitResult=bash_gadget::runShellCommand(managed,poller,
      shell::RunOptions{.command="sleep 0.2; echo explicit",.timeoutMs=5000},
      explicitOptions,callbacks());
    check(explicitResult.backgroundMissionId.has_value()&&
      events.back()=="spawn:"+*explicitResult.backgroundMissionId,
      "explicit background did not register before returning");
    check(managed->wait(*explicitResult.backgroundMissionId,2000).stdoutText.find("explicit")!=std::string::npos,
      "explicit background did not survive tool return");
    managed->cleanup(*explicitResult.backgroundMissionId);events.clear();
    auto slowSettings=bash_gadget::RunShellSettings{};slowSettings.showBackgroundUI=true;
    const auto slow=bash_gadget::runShellCommand(managed,poller,
      shell::RunOptions{.command="echo ready; sleep 3.2; echo done",.timeoutMs=5000},
      slowSettings,callbacks());
    check(slow.stdoutText.find("done")!=std::string::npos,"slow foreground output lost");
    check(events.size()>=4&&events[0].rfind("foreground:",0)==0&&events[1]=="hint"&&
      events[2].rfind("progress:",0)==0&&events.back().rfind("unregister:",0)==0,
      "foreground progress/UI/unregister order changed");
    events.clear();
    auto assistant=bash_gadget::RunShellSettings{};
    assistant.assistantMode=true;assistant.assistantBlockingBudgetMs=90;
    assistant.progressThresholdMs=20;
    const auto autoBg=bash_gadget::runShellCommand(managed,poller,
      shell::RunOptions{.command="sleep 0.4; echo assistant",.timeoutMs=5000},
      assistant,callbacks());
    check(autoBg.backgroundMissionId.has_value()&&autoBg.assistantAutoBackgrounded,
      "assistant budget did not background command");
    check(managed->wait(*autoBg.backgroundMissionId,2000).stdoutText.find("assistant")!=std::string::npos,
      "assistant background output lost");
    managed->cleanup(*autoBg.backgroundMissionId);events.clear();
    const auto timed=bash_gadget::runShellCommand(managed,poller,
      shell::RunOptions{.command="sleep 0.4; echo timeout",.timeoutMs=60},
      bash_gadget::RunShellSettings{.progressThresholdMs=20},callbacks());
    check(timed.backgroundMissionId.has_value(),"timeout did not background allowed command");
    check(managed->wait(*timed.backgroundMissionId,2000).stdoutText.find("timeout")!=std::string::npos,
      "timeout background output lost");
    managed->cleanup(*timed.backgroundMissionId);
    const auto immediateTimeout=bash_gadget::runShellCommand(managed,poller,
      shell::RunOptions{.command="sleep 0.2; echo immediate",.timeoutMs=1},
      bash_gadget::RunShellSettings{.progressThresholdMs=20},callbacks());
    check(immediateTimeout.backgroundMissionId.has_value(),
      "timeout callback registration lost race with native worker");
    managed->wait(*immediateTimeout.backgroundMissionId,2000);
    managed->cleanup(*immediateTimeout.backgroundMissionId);
    events.clear();
    auto raceCallbacks=callbacks();
    std::shared_future<shell::ExecResult> raceCompletion;
    raceCallbacks.registerForeground=[&](const shell::CommandHandle& handle){
      events.push_back("foreground:"+handle.missionId());
      check(handle.background(handle.missionId(),true),"simulated Ctrl+B did not background");
      raceCompletion=handle.result();
      return handle.missionId();};
    raceCallbacks.yieldProgress=[&](const shell::Progress&){
      // Complete while progress is being delivered, before the next loop poll.
      // A fixed sleep cannot reliably produce this ordering on every platform.
      if(raceCompletion.valid())check(
        raceCompletion.wait_for(std::chrono::seconds(4))==std::future_status::ready,
        "background command did not complete during progress delivery");};
    auto raceSettings=bash_gadget::RunShellSettings{};
    raceSettings.progressThresholdMs=20;raceSettings.showBackgroundUI=true;
    const auto raced=bash_gadget::runShellCommand(managed,poller,
      shell::RunOptions{.command="printf '%040000d' 0; sleep 1.2",.timeoutMs=5000},
      raceSettings,raceCallbacks);
    check(!raced.backgroundMissionId&&raced.outputFilePath&&raced.outputFileSize==40000&&
      std::filesystem::exists(*raced.outputFilePath)&&
      std::find_if(events.begin(),events.end(),[](const std::string& event){
        return event.rfind("notified:",0)==0;})!=events.end(),
      "completion/background race did not repair large output and suppress notification");
    std::filesystem::remove(*raced.outputFilePath);
    std::filesystem::remove_all(root);
    std::cout<<"PASS runShellCommand threshold/progress/background/timeout order\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
