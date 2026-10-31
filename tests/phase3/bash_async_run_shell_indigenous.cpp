#include <cstdlib>
#include "gadgets/bash/AsyncRunShellCommand.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unistd.h>
#include <vector>
using namespace batbox;
using namespace batbox::bash_gadget;
using namespace batbox::event_loop;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
  std::string pattern="/tmp/batbox-async-run-shell-XXXXXX";
  std::vector<char> chars(pattern.begin(),pattern.end());chars.push_back('\0');
  const auto made=::mkdtemp(chars.data());require(made,"mkdtemp failed");
  const std::filesystem::path root=made;
  try{
    Loop loop;shell::MissionOutputPoller poller;
    shell::FlightConfig config;config.shellPath="/bin/bash";
    config.originalCwd=root.string();config.outputDir=(root/"jobs").string();
    config.generateMissionId=[]{return std::string("async-shell-1");};
    config.splitCommand=[](const std::string&){return std::vector<std::string>{"printf"};};
    auto shell=std::make_shared<shell::ManagedShell>(std::move(config));
    AsyncRunImports imports;
    imports.exec=bindManagedExec(loop,shell,shell::RunOptions{});
    imports.startPolling=bindMissionOutputPoller(loop,poller);
    imports.spawnBackgroundMission=[](const AsyncRunInput&,
      const AsyncShellCommand&)->Deferred<std::string>{
      throw std::runtime_error("unexpected background task");};
    imports.backgroundExistingForegroundMission=[](const std::string&,
      const AsyncRunInput&,
      const AsyncShellCommand&){throw std::runtime_error("unexpected foreground task");
      return false;};
    imports.registerForeground=[](const AsyncRunInput&,
      const AsyncShellCommand&)->std::string{
      throw std::runtime_error("unexpected foreground register");};
    imports.unregisterForeground=[](const std::string&){
      throw std::runtime_error("unexpected unregister");};
    imports.markMissionNotified=[](const std::string&){
      throw std::runtime_error("unexpected notification repair");};
    imports.isAutoBackgroundAllowed=[](const std::string&){return true;};
    imports.shouldUseSandbox=[](const AsyncRunInput&){return false;};
    const auto started=std::chrono::steady_clock::now();
    imports.nowMs=[started]{return std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-started).count();};
    imports.getDefaultTimeoutMs=[]{return 5000.0;};
    imports.onUnobservedError=[](std::exception_ptr error){std::rethrow_exception(error);};
    RunShellSettings settings;settings.progressThresholdMs=20;
    const auto progressObserved=root/"progress-observed";
    AsyncRunInput input;input.command="printf 'hello\\n'; while [ ! -e '"+
      progressObserved.string()+"' ]; do sleep 0.02; done";
    AsyncRunShellCommand generator(loop,input,settings,imports);
    std::vector<shell::Progress> progress;std::optional<shell::ExecResult> final;
    std::function<void()> next;
    next=[&]{generator.next().then([&](const AsyncRunStep* step,
      const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      if(step->done){final=step->result;return;}
      progress.push_back(*step->progress);
      // Let the real process finish only after its output reaches the owner.
      // An initial empty poll is valid while the child is starting up.
      if(step->progress->fullOutput.find("hello")!=std::string::npos)
        std::ofstream(progressObserved)<<"observed\n";
      next();
    });};
    next();loop.run();
    require(final&&final->code==0&&final->stdoutText.find("hello")!=std::string::npos,
      "real Bash generator result missing");
    require(std::any_of(progress.begin(),progress.end(),[](const auto& sample){
      return sample.fullOutput.find("hello")!=std::string::npos;}),
      "real poller progress missing");
  }catch(...){std::filesystem::remove_all(root);throw;}
  std::filesystem::remove_all(root);
}
