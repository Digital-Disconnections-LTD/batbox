#include "gadgets/bash/AsyncRunShellCommand.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <iostream>
#include <memory>
using namespace batbox;
using namespace batbox::event_loop;
using namespace batbox::bash_gadget;
namespace {
Json eventList(const std::vector<std::string>& events){Json value=Json::array();
  for(const auto& event:events)value.push_back(event);return value;}
Json stepValue(const AsyncRunStep& step){
  return step.toJson();
}
Json run(const std::string& mode){
  Loop loop;std::vector<std::string> events;
  Deferred<shell::ExecResult> commandResult(loop);
  auto status=std::make_shared<shell::CommandStatus>(shell::CommandStatus::Running);
  shell::CommandHandle::TimeoutCallback timeoutCallback;
  std::function<void(const shell::PollSample&)> progressCallback;
  const auto started=std::chrono::steady_clock::now();
  AsyncRunImports imports;
  imports.exec=[&](const AsyncExecRequest&){
    AsyncShellCommand command(commandResult);
    command.missionId="task1";command.outputPath="task1.output";
    command.status=[status]{return *status;};
    command.cleanup=[&]{events.push_back("cleanup");};
    command.background=[&events,status](const std::string& id){
      events.push_back("background:"+id);*status=shell::CommandStatus::Backgrounded;return true;};
    command.onTimeout=[&](const auto& callback){timeoutCallback=callback;};
    command.largeOutputInfo=[] {return std::optional<shell::CommandHandle::LargeOutputInfo>(
      shell::CommandHandle::LargeOutputInfo{"task1.output",100,"task1"});};
    Deferred<AsyncShellCommand> ready(loop);ready.resolve(command);return ready;
  };
  imports.spawnBackgroundMission=[&](const AsyncRunInput&,const AsyncShellCommand& command){
    events.push_back("spawn");command.background("task1");
    Deferred<std::string> ready(loop);ready.resolve("task1");return ready;
  };
  imports.backgroundExistingForegroundMission=[](const std::string&,
    const AsyncRunInput&,const AsyncShellCommand&){
    throw std::runtime_error("unexpected foreground background");return false;};
  imports.registerForeground=[&](const AsyncRunInput&,
    const AsyncShellCommand&){events.push_back("register");
    return std::string("task1");};
  imports.unregisterForeground=[&](const std::string&){events.push_back("unregister");};
  imports.markMissionNotified=[&](const std::string&){events.push_back("mark");};
  imports.showBackgroundHint=[&]{events.push_back("hint");};
  imports.startPolling=[&](const AsyncShellCommand&,
    std::function<void(const shell::PollSample&)> progress,
    std::function<void(std::exception_ptr)>){
    events.push_back("poll-start");progressCallback=std::move(progress);
    if(mode=="demand"||mode=="concurrent"||mode=="userBackground")loop.setTimeout(5,[&]{
      progressCallback(shell::PollSample{"hello\n","hello\n",2,6,false});});
    return std::function<void()>([&]{events.push_back("poll-stop");});
  };
  imports.isAutoBackgroundAllowed=[](const std::string&){return true;};
  imports.shouldUseSandbox=[](const AsyncRunInput&){return false;};
  imports.nowMs=[started]{return std::chrono::duration<double,std::milli>(
    std::chrono::steady_clock::now()-started).count();};
  imports.getDefaultTimeoutMs=[]{return 5000.0;};
  imports.onUnobservedError=[](std::exception_ptr error){std::rethrow_exception(error);};
  RunShellSettings settings;settings.progressThresholdMs=mode=="userBackground"?0:1;
  settings.showBackgroundUI=mode=="userBackground";
  AsyncRunInput input;input.command="sleep 1";input.description="test";
  AsyncRunShellCommand generator(loop,input,settings,imports);
  if(mode=="backgroundWake")loop.setTimeout(5,[&]{
    timeoutCallback([&](const std::string& id){events.push_back("background:"+id);
      *status=shell::CommandStatus::Backgrounded;return true;});
  });
  if(mode=="repair")loop.setTimeout(8,[&]{
    events.push_back("background:task1");*status=shell::CommandStatus::Backgrounded;
    shell::ExecResult value;value.stdoutText="done";value.backgroundMissionId="task1";
    commandResult.resolve(value);
  });
  if(mode=="demand"||mode=="concurrent")loop.setTimeout(12,[&]{shell::ExecResult value;value.stdoutText="done";
    commandResult.resolve(value);});
  auto guard=mode=="userBackground"?
    std::optional(loop.setTimeout(100,[]{})):std::nullopt;
  Json first,second,beforeResume;
  auto firstRequest=generator.next();
  if(mode=="concurrent")generator.next().then([&](const AsyncRunStep* step,
    const std::exception_ptr& error){if(error)std::rethrow_exception(error);
    second=stepValue(*step);});
  firstRequest.then([&](const AsyncRunStep* step,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    first=stepValue(*step);
    if(mode=="demand"){
      loop.setTimeout(25,[&]{beforeResume=eventList(events);
        generator.next().then([&](const AsyncRunStep* next,
          const std::exception_ptr& error){if(error)std::rethrow_exception(error);
          second=stepValue(*next);});});
    }else if(mode=="userBackground"){
      events.push_back("background:task1");*status=shell::CommandStatus::Backgrounded;
      loop.setTimeout(5,[&]{progressCallback(shell::PollSample{
        "hello\n","hello\n",2,6,false});});
      generator.next().then([&](const AsyncRunStep* next,
        const std::exception_ptr& error){if(error)std::rethrow_exception(error);
        second=stepValue(*next);if(guard)guard->cancel();});
    }
  });
  loop.run();
  Json output={{"first",first},{"events",eventList(events)}};
  if(mode=="demand"){output["beforeResume"]=beforeResume;output["second"]=second;}
  if(mode=="concurrent")output["second"]=second;
  if(mode=="userBackground")output["second"]=second;
  return output;
}
}
int main(){Json output=Json::object();
  for(const std::string mode:{"demand","concurrent","backgroundWake","userBackground","repair"})
    output[mode]=run(mode);
  std::cout<<value::stringify(output)<<'\n';}
