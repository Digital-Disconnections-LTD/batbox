#include "gadgets/bash/AsyncRunShellCommand.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <iostream>
#include <memory>
using namespace batbox;
using namespace batbox::event_loop;
using namespace batbox::bash_gadget;
namespace {
Json run(const std::string& mode){
  Loop loop;std::vector<std::string> events;Json progress=Json::array();
  auto status=std::make_shared<shell::CommandStatus>(shell::CommandStatus::Running);
  auto started=std::chrono::steady_clock::now();
  const auto now=[&]{return std::chrono::duration<double,std::milli>(
    std::chrono::steady_clock::now()-started).count();};
  AsyncRunImports imports;
  imports.exec=[&](const AsyncExecRequest&){
    Deferred<shell::ExecResult> result(loop);
    if(mode=="quick"){
      shell::ExecResult value;value.stdoutText="quick\n";result.resolve(value);
    }else if(mode=="slow"){
      loop.setTimeout(1300,[result]{shell::ExecResult value;value.stdoutText="hello\n";
        result.resolve(value);});
    }
    AsyncShellCommand command(result);command.missionId="task1";command.outputPath="task1.output";
    command.status=[status]{return *status;};
    command.cleanup=[&]{events.push_back("cleanup");};
    command.background=[&events,status](const std::string& id){
      events.push_back("background:"+id);*status=shell::CommandStatus::Backgrounded;return true;};
    command.onTimeout=[](const shell::CommandHandle::TimeoutCallback&){};
    command.largeOutputInfo=[]{return std::optional<shell::CommandHandle::LargeOutputInfo>{};};
    Deferred<AsyncShellCommand> ready(loop);ready.resolve(command);return ready;
  };
  imports.spawnBackgroundMission=[&](const AsyncRunInput&,const AsyncShellCommand& command){
    events.push_back("spawn");command.background(command.missionId);
    Deferred<std::string> ready(loop);ready.resolve(command.missionId);return ready;
  };
  imports.backgroundExistingForegroundMission=[](const std::string&,
    const AsyncRunInput&,const AsyncShellCommand&){
    throw std::runtime_error("unexpected foreground background");return false;};
  imports.registerForeground=[](const AsyncRunInput&,const AsyncShellCommand&)->std::string{
    throw std::runtime_error("unexpected foreground register");};
  imports.unregisterForeground=[](const std::string&){throw std::runtime_error("unexpected unregister");};
  imports.markMissionNotified=[](const std::string&){throw std::runtime_error("unexpected mark");};
  imports.startPolling=[&](const AsyncShellCommand&,
    std::function<void(const shell::PollSample&)> onProgress,
    std::function<void(std::exception_ptr)>){
    if(mode!="slow")throw std::runtime_error("unexpected poll");
    auto active=std::make_shared<bool>(true);
    loop.setTimeout(1000,[active,onProgress]{
      if(*active)onProgress(shell::PollSample{"hello\n","hello\n",2,6,false});});
    return std::function<void()>([active]{*active=false;});
  };
  imports.isAutoBackgroundAllowed=[](const std::string&){return true;};
  imports.shouldUseSandbox=[](const AsyncRunInput&){return false;};
  imports.nowMs=now;imports.getDefaultTimeoutMs=[]{return 5000.0;};
  imports.onUnobservedError=[](std::exception_ptr error){std::rethrow_exception(error);};
  RunShellSettings settings;settings.progressThresholdMs=mode=="slow"?20:2000;
  AsyncRunInput input;input.command=mode=="quick"?"echo quick":
    mode=="slow"?"echo hello; sleep 1.3":"sleep 1";
  input.description="test";input.runInBackground=mode=="explicit";
  AsyncRunShellCommand generator(loop,input,settings,imports);
  Json value;
  std::function<void()> request;
  request=[&]{generator.next().then([&](const AsyncRunStep* step,
    const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    if(step->done){value=step->result?step->result->toJson():Json();return;}
    progress.push_back(step->progress->toJson());request();
  });};
  request();loop.run();
  Json result={{"value",value},{"events",Json::array()},{"progress",progress}};
  for(const auto& event:events)result["events"].push_back(event);
  return result;
}
}
int main(){Json output=Json::array();
  for(const std::string mode:{"quick","explicit","slow"})output.push_back(run(mode));
  std::cout<<value::stringify(output)<<'\n';}
