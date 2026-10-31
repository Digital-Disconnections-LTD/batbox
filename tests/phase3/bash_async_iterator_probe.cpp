#include "gadgets/bash/AsyncRunShellCommand.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <iostream>
#include <memory>
using namespace batbox;
using namespace batbox::event_loop;
using namespace batbox::bash_gadget;
namespace {
Json eventsJson(const std::vector<std::string>& events){Json out=Json::array();
  for(const auto& value:events)out.push_back(value);return out;}
Json observed(const AsyncRunStep* step,const std::exception_ptr& error){
  if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
    return Json{{"ok",false},{"error",e.what()}};}}
  return Json{{"ok",true},{"step",step->toJson()}};
}
shell::ExecResult result(const std::string& text){shell::ExecResult value;
  value.stdoutText=text;return value;}
Json run(const std::string& mode){
  Loop loop;std::vector<std::string> events;
  Deferred<shell::ExecResult> commandResult(loop);
  auto status=std::make_shared<shell::CommandStatus>(shell::CommandStatus::Running);
  std::function<void(const shell::PollSample&)> progressCallback;
  const auto started=std::chrono::steady_clock::now();
  AsyncRunImports imports;
  imports.exec=[&](const AsyncExecRequest&){events.push_back("exec");
    AsyncShellCommand command(commandResult);
    command.missionId="task1";command.outputPath="task1.output";
    command.status=[status]{return *status;};
    command.cleanup=[&]{events.push_back("cleanup");};
    command.background=[&events,status](const std::string& id){
      events.push_back("background:"+id);*status=shell::CommandStatus::Backgrounded;
      return true;};
    command.onTimeout=[](const shell::CommandHandle::TimeoutCallback&){};
    command.largeOutputInfo=[] {return std::optional<shell::CommandHandle::LargeOutputInfo>{};};
    Deferred<AsyncShellCommand> ready(loop);ready.resolve(command);return ready;
  };
  imports.spawnBackgroundMission=[&](const AsyncRunInput&,const AsyncShellCommand& command){
    events.push_back("spawn");command.background("task1");
    Deferred<std::string> ready(loop);ready.resolve("task1");return ready;
  };
  imports.backgroundExistingForegroundMission=[](const std::string&,
    const AsyncRunInput&,const AsyncShellCommand&){throw std::runtime_error("unexpected");
    return false;};
  imports.registerForeground=[](const AsyncRunInput&,
    const AsyncShellCommand&)->std::string{throw std::runtime_error("unexpected");};
  imports.unregisterForeground=[&](const std::string&){events.push_back("unregister");};
  imports.markMissionNotified=[&](const std::string&){events.push_back("mark");};
  imports.startPolling=[&](const AsyncShellCommand&,
    std::function<void(const shell::PollSample&)> progress,
    std::function<void(std::exception_ptr)>){
    events.push_back("poll-start");progressCallback=std::move(progress);
    if(mode!="settledReturn")loop.setTimeout(5,[&]{
      progressCallback(shell::PollSample{"hello\n","hello\n",2,6,false});});
    return std::function<void()>([&]{events.push_back("poll-stop");});
  };
  imports.isAutoBackgroundAllowed=[](const std::string&){return true;};
  imports.shouldUseSandbox=[](const AsyncRunInput&){return false;};
  imports.nowMs=[started]{return std::chrono::duration<double,std::milli>(
    std::chrono::steady_clock::now()-started).count();};
  imports.getDefaultTimeoutMs=[]{return 5000.0;};
  imports.onUnobservedError=[](std::exception_ptr error){std::rethrow_exception(error);};
  RunShellSettings settings;settings.progressThresholdMs=mode=="settledReturn"?20:1;
  AsyncRunInput input;input.command="sleep 1";input.description="test";
  input.runInBackground=mode=="backgroundReturn";
  AsyncRunShellCommand generator(loop,input,settings,imports);
  Json first,last,middle;
  const auto thrown=std::make_exception_ptr(std::runtime_error("forced"));
  if(mode=="preReturn"||mode=="preReturnUndefined"||mode=="preThrow"){
    const auto request=mode=="preThrow"?generator.throwError(thrown):
      mode=="preReturnUndefined"?generator.returnValue():
      generator.returnValue(result("return"));
    request.then([&](const AsyncRunStep* step,const std::exception_ptr& error){
      last=observed(step,error);});
    Json after;
    if(mode=="preReturnUndefined")generator.next().then([&](const AsyncRunStep* step,
      const std::exception_ptr& error){after=observed(step,error);});
    loop.run();
    Json output={{"last",last},{"events",eventsJson(events)}};
    if(mode=="preReturnUndefined")output["after"]=after;
    return output;
  }
  auto guard=loop.setTimeout(75,[]{});
  if(mode=="settledReturn")loop.setTimeout(0,[commandResult]{
    commandResult.resolve(result("done"));});
  if(mode=="pendingReturn"||mode=="pendingThrow"||mode=="yieldNextReturn")
    loop.setTimeout(12,[commandResult]{commandResult.resolve(result("done"));});
  const auto attachLast=[&](Deferred<AsyncRunStep> request){
    request.then([&](const AsyncRunStep* step,const std::exception_ptr& error){
      last=observed(step,error);guard.cancel();});
  };
  const auto lastRequest=[&]{return mode=="pendingThrow"||mode=="yieldThrow"?
    generator.throwError(thrown):generator.returnValue(result("return"));};
  const auto firstRequest=generator.next();
  if(mode=="pendingReturn"||mode=="pendingThrow"||mode=="settledReturn"||
    mode=="backgroundReturn")attachLast(lastRequest());
  firstRequest.then([&](const AsyncRunStep* step,const std::exception_ptr& error){
    first=observed(step,error);
    if(mode=="yieldReturn"||mode=="yieldThrow")attachLast(lastRequest());
    if(mode=="yieldNextReturn"){
      generator.next().then([&](const AsyncRunStep* step,
        const std::exception_ptr& error){middle=observed(step,error);});
      attachLast(lastRequest());
    }
  });
  loop.run();
  Json output={{"first",first},{"last",last},{"events",eventsJson(events)}};
  if(mode=="yieldNextReturn")output["middle"]=middle;
  return output;
}
}
int main(){Json output=Json::object();
  for(const std::string mode:{"preReturn","preReturnUndefined","preThrow",
    "yieldReturn","yieldThrow",
    "pendingReturn","pendingThrow","settledReturn","yieldNextReturn",
    "backgroundReturn"})output[mode]=run(mode);
  std::cout<<value::stringify(output)<<'\n';}
