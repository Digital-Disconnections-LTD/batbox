#include <cstdlib>
#include "gadgets/bash/AsyncRunShellCommand.hpp"
#include "shell/CommandSplitter.hpp"
#include <chrono>
#include <filesystem>
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
  std::string pattern="/tmp/batbox-async-iterator-XXXXXX";
  std::vector<char> chars(pattern.begin(),pattern.end());chars.push_back('\0');
  const auto made=::mkdtemp(chars.data());require(made,"mkdtemp failed");
  const std::filesystem::path root=made;
  try{
    Loop loop;shell::MissionOutputPoller poller;
    shell::FlightConfig config;config.shellPath="/bin/bash";
    config.originalCwd=root.string();config.outputDir=(root/"jobs").string();
    config.generateMissionId=[]{return std::string("async-iterator-1");};
    config.splitCommand=[](const std::string& command){
      return shell_split::splitCommandWithOperators(command,
        []{return std::string("native-iterator-salt");});
    };
    auto shell=std::make_shared<shell::ManagedShell>(std::move(config));
    AsyncRunImports imports;
    auto managedExec=bindManagedExec(loop,shell,shell::RunOptions{});
    bool childFinished=false;std::string childOutput;
    imports.exec=[&](const AsyncExecRequest& request){
      auto pending=managedExec(request);
      pending.then([&](const AsyncShellCommand* command,const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        command->result.then([&](const shell::ExecResult* result,
          const std::exception_ptr& error){
          if(error)std::rethrow_exception(error);
          childFinished=true;childOutput=result->stdoutText;
        });
      });
      return pending;
    };
    auto poll=bindMissionOutputPoller(loop,poller);
    bool stopped=false;
    imports.startPolling=[&](const AsyncShellCommand& command,
      std::function<void(const shell::PollSample&)> progress,
      std::function<void(std::exception_ptr)> error){
      auto stop=poll(command,std::move(progress),std::move(error));
      return std::function<void()>([stop=std::move(stop),&stopped]{stop();stopped=true;});
    };
    imports.spawnBackgroundMission=[](const AsyncRunInput&,
      const AsyncShellCommand&)->Deferred<std::string>{
      throw std::runtime_error("unexpected background task");};
    imports.backgroundExistingForegroundMission=[](const std::string&,
      const AsyncRunInput&,const AsyncShellCommand&){
      throw std::runtime_error("unexpected foreground task");return false;};
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
    AsyncRunInput input;input.command="printf 'early\\n'; sleep 1.3; printf 'late\\n'";
    AsyncRunShellCommand generator(loop,input,settings,imports);
    bool sawProgress=false,returned=false;
    generator.next().then([&](const AsyncRunStep* step,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      require(!step->done,"command finished before progress");
      sawProgress=true;
      generator.returnValue().then([&](const AsyncRunStep* closed,
        const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        require(closed->done&&!closed->result,"return() did not close iterator");
        require(!childFinished,"return() waited for the child to finish");
        returned=true;
      });
    });
    loop.run();
    require(sawProgress&&returned&&stopped,"iterator did not stop the poller");
    require(childFinished&&childOutput.find("late")!=std::string::npos,
      "return() stopped the child process");
  }catch(...){std::filesystem::remove_all(root);throw;}
  std::filesystem::remove_all(root);
}
