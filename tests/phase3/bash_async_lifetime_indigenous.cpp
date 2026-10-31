#include <cstdlib>
#include "gadgets/bash/AsyncRunShellCommand.hpp"
#include "shell/CommandSplitter.hpp"
#include "shell/LoopMissionOutput.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace batbox;
using namespace batbox::bash_gadget;
using namespace batbox::event_loop;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
  std::string pattern="/tmp/batbox-async-lifetime-XXXXXX";
  std::vector<char> chars(pattern.begin(),pattern.end());chars.push_back('\0');
  const auto made=::mkdtemp(chars.data());check(made,"mkdtemp failed");
  const std::filesystem::path root=made;
  try{
    Loop loop;std::atomic<int> ids{0};
    shell::FlightConfig config;config.shellPath="/bin/bash";
    config.originalCwd=root.string();config.outputDir=(root/"jobs").string();
    config.generateMissionId=[&]{return "lifetime-"+std::to_string(++ids);};
    config.splitCommand=[](const std::string& command){
      return shell_split::splitCommandWithOperators(command,
        []{return std::string("lifetime-salt");});};
    auto managed=std::make_shared<shell::ManagedShell>(std::move(config));
    for(int i=0;i<12;i++){
      std::weak_ptr<void> stateLife;
      std::weak_ptr<AsyncShellCommand> holderLife;
      std::string missionId;
      {
        shell::RunOptions options;options.command="sleep 0.03";
        options.autoBackgroundEnabled=false;
        auto handle=shell::startManagedCommand(managed,std::move(options),false);
        missionId=handle.missionId();stateLife=handle.lifetimeToken();
        auto holder=std::make_shared<AsyncShellCommand>(bindAsyncShellCommand(loop,handle));
        holderLife=holder;
        // Reproduce the former State -> callback -> bound handle -> State
        // cycle, plus a generator-style callback retaining its owner.
        holder->onTimeout([holder](const auto&){});
      }
      loop.run();
      check(stateLife.expired()&&holderLife.expired(),
        "running command retained timeout callback ownership cycle");
      (void)managed->release(missionId);
    }
    shell::MissionOutputPoller poller;
    AsyncRunImports imports;
    imports.exec=bindManagedExec(loop,managed,shell::RunOptions{});
    imports.startPolling=bindMissionOutputPoller(loop,poller);
    imports.spawnBackgroundMission=[](const AsyncRunInput&,
      const AsyncShellCommand&)->Deferred<std::string>{
      throw std::runtime_error("unexpected background");};
    imports.backgroundExistingForegroundMission=[](const std::string&,
      const AsyncRunInput&,const AsyncShellCommand&){return false;};
    imports.registerForeground=[](const AsyncRunInput&,
      const AsyncShellCommand&)->std::string{throw std::runtime_error("unexpected foreground");};
    imports.unregisterForeground=[](const std::string&){};
    imports.markMissionNotified=[](const std::string&){};
    imports.isAutoBackgroundAllowed=[](const std::string&){return true;};
    imports.shouldUseSandbox=[](const AsyncRunInput&){return false;};
    imports.nowMs=[] {return 0.0;};imports.getDefaultTimeoutMs=[] {return 5000.0;};
    imports.onUnobservedError=[](std::exception_ptr error){std::rethrow_exception(error);};
    for(int i=0;i<12;i++){
      AsyncRunInput input;input.command="sleep 0.03";
      RunShellSettings settings;settings.progressThresholdMs=2000;
      auto generator=std::make_unique<AsyncRunShellCommand>(loop,input,settings,imports);
      auto life=generator->lifetimeToken();bool finished=false;
      generator->next().then([&](const AsyncRunStep* step,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        check(step->done&&step->result.has_value(),"generator command did not finish");
        finished=true;
      });
      loop.run();generator.reset();
      check(finished&&life.expired(),"generator retained timeout callback cycle");
    }
    for(int i=0;i<12;i++){
      std::weak_ptr<void> stateLife;
      std::weak_ptr<AsyncShellCommand> holderLife;
      std::string missionId;
      {
        shell::RunOptions options;options.command="true";
        options.autoBackgroundEnabled=false;
        auto handle=shell::startManagedCommand(managed,std::move(options),false);
        missionId=handle.missionId();stateLife=handle.lifetimeToken();
        handle.result().wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        auto holder=std::make_shared<AsyncShellCommand>(bindAsyncShellCommand(loop,handle));
        holderLife=holder;
        holder->onTimeout([holder](const auto&){});
      }
      loop.run();
      check(stateLife.expired()&&holderLife.expired(),
        "late timeout registration retained completed command");
      (void)managed->release(missionId);
    }
  }catch(...){std::filesystem::remove_all(root);throw;}
  std::filesystem::remove_all(root);
}
