#include <cstdlib>
#include "gadgets/bash/AsyncBashCall.hpp"
#include "shell/CommandSplitter.hpp"
#include "missions/shell/Roster.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace batbox;
using namespace batbox::bash_gadget;
using namespace batbox::event_loop;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();
}
}
int main(){
  std::string pattern="/tmp/batbox-async-tool-XXXXXX";
  std::vector<char> chars(pattern.begin(),pattern.end());chars.push_back('\0');
  const auto made=::mkdtemp(chars.data());check(made,"mkdtemp failed");
  const std::filesystem::path root=made;
  try{
    Loop loop;
    std::atomic<int> ids{0};std::mutex notificationMutex;
    std::vector<std::string> notifications;
    shell_missions::Services missionServices;
    missionServices.nowMs=[] {return 1234LL;};
    missionServices.enqueueNotification=[&](const std::string& message,
      const std::optional<std::string>&){std::lock_guard lock(notificationMutex);
      notifications.push_back(message);};
    missionServices.onMissionTerminated=[](const shell_missions::MissionRecord&){};
    auto roster=std::make_shared<shell_missions::Roster>(missionServices);
    shell::FlightConfig config;config.shellPath="/bin/bash";
    config.originalCwd=root.string();config.outputDir=(root/"jobs").string();
    config.maxInlineOutputBytes=100;
    config.generateMissionId=[&]{return "async-task-"+std::to_string(++ids);};
    config.splitCommand=[](const std::string& command){
      return shell_split::splitCommandWithOperators(command,
        []{return std::string("async-tool-salt");});};
    config.onBackgroundComplete=[weak=std::weak_ptr(roster)](
      const std::string& id,const shell::ExecResult& result){
      if(auto missions=weak.lock())missions->completeShell(id,result);};
    Services services;services.shell=std::make_shared<shell::ManagedShell>(std::move(config));
    services.missionOutputPoller=std::make_shared<shell::MissionOutputPoller>();
    roster->attachShell(services.shell);
    services.originalCwd=root.string();
    bindIndigenousCommandSplitters(services);bindIndigenousBashSchema(services,600000);
    std::vector<std::string> hintsLog;
    bindIndigenousHintParser(services,{"batbox-hint",[&](const std::string& text){
      hintsLog.push_back(text);}});
    services.poetry=[](const gadgets::PoetryContext&){return ready(std::string("Run shell commands"));};
    services.checkPermissions=[](const Json& input,const Json&){
      return ready(Json{{"behavior","allow"},{"updatedInput",input}});};
    services.readOnlyAllowed=[](const Json&,bool hasCd){return !hasCd;};
    services.commandHasAnyCd=[](const std::string& command){
      return command.find("cd ")!=std::string::npos;};
    services.preparePermissionMatcher=[](const Json&){
      return ready(std::function<bool(const std::string&)>(
        [](const std::string&){return true;}));};
    services.shouldUseSandbox=[](const Json&){return false;};
    services.sandboxCommand=[](const std::string& command,const Json&,const Json&){return command;};
    services.annotateSandboxFailures=[](const std::string&,
      const std::string& output){return output;};
    services.resetCwdIfOutsideProject=[](const Json&){return false;};
    std::vector<Json> recordedHints;
    services.recordHint=[&](const Json& hint){recordedHints.push_back(hint);};
    services.resizeImage=[](const std::string&,
      const shell::ExecResult&){return std::optional<std::string>{};};
    services.applySedEdit=[](const Json&,const gadgets::GadgetCallContext&,
      const Json&)->gadgets::GadgetResult{throw std::runtime_error("unexpected sed edit");};
    services.parseSedEdit=[](const std::string&){return Json();};
    services.fileEditUserFacingName=[](const Json&){return std::string("Edit");};
    services.truncateSummary=[](const std::string& value,std::size_t max){
      return value.substr(0,max);};
    services.getGadgetResultPath=[root](const std::string& id){
      return (root/"results"/(id+".txt")).string();};
    services.registerBackgroundMission=[roster](const std::string& id,
      const std::string& command,const std::string& description,const Json&){
      shell_missions::MissionRecord record;record.id=id;record.command=command;
      record.description=description;roster->adoptBackground(std::move(record));};
    services.registerForegroundMission=[roster](const std::string& id,
      const std::string& command,const std::string& description,const Json&){
      shell_missions::MissionRecord record;record.id=id;record.command=command;
      record.description=description;roster->registerForeground(std::move(record));};
    services.unregisterForegroundMission=[roster](const std::string& id){
      (void)roster->unregisterForeground(id);};
    services.backgroundExistingForegroundMission=[roster](const std::string& id){
      return roster->backgroundExistingForeground(id);};
    services.markMissionNotified=[roster](const std::string& id){
      (void)roster->markNotified(id);};
    services.renderUse=[](const Json&,const Json&){return Json("use");};
    services.renderResult=[](const Json&,const Json&,const Json&){return Json("result");};
    services.renderProgress=[](const Json&,const Json&){return Json("progress");};
    services.renderQueued=[](){return Json("queued");};
    services.renderError=[](const Json&,const Json&){return Json("error");};
    AsyncMissionHost host;
    host.spawnShellMission=[&](const AsyncRunInput& input,
      const AsyncShellCommand& command,const gadgets::GadgetCallContext&){
      shell_missions::MissionRecord record;record.id=command.missionId;
      record.command=input.command;
      record.description=input.description.empty()?input.command:input.description;
      record.gadgetUseId=input.gadgetUseId;record.batId=input.batId;
      roster->adoptBackground(std::move(record));
      check(command.background(command.missionId),"background transition failed");
      Deferred<std::string> done(loop);done.resolve(command.missionId);return done;
    };
    host.backgroundExistingForegroundMission=[roster](const std::string& id,
      const AsyncRunInput&,const AsyncShellCommand&,const gadgets::GadgetCallContext&){
      return roster->backgroundExistingForeground(id);};
    host.registerForeground=[roster](const AsyncRunInput& input,
      const AsyncShellCommand& command,const gadgets::GadgetCallContext&){
      shell_missions::MissionRecord record;record.id=command.missionId;record.command=input.command;
      record.description=input.description.empty()?input.command:input.description;
      roster->registerForeground(std::move(record));return command.missionId;};
    host.unregisterForeground=[roster](const std::string& id,
      const gadgets::GadgetCallContext&){(void)roster->unregisterForeground(id);};
    host.markMissionNotified=[roster](const std::string& id,
      const gadgets::GadgetCallContext&){(void)roster->markNotified(id);};
    auto splitter=services.splitCommand;
    host.isAutoBackgroundAllowed=[splitter](const std::string& command){
      return shell::isAutobackgroundingAllowed(command,splitter);};
    host.getDefaultTimeoutMs=[] {return 5000.0;};
    host.getProgressThresholdMs=[] {return 20.0;};
    host.getGadgetPermissionContext=[] {return Json::object();};
    host.onUnobservedError=[](std::exception_ptr error){std::rethrow_exception(error);};
    host.fileOps=indigenousBashFileOps(loop,(root/"results").string(),
      [root](const std::string& id,bool){return (root/"results"/(id+".txt")).string();});
    auto gadget=makeAsyncBashGadget(std::move(services),loop,std::move(host));
    gadgets::GadgetCallContext context;context.data=Json{{"toolUseId","toolu_async"}};
    auto quick=gadget.call(Json{{"command","printf 'hello\\n'"}},context,{},Json(),{});
    check(quick.wait_for(std::chrono::milliseconds(0))==std::future_status::timeout,
      "Tool.call blocked during quick setup");
    loop.run();check(quick.get().data["stdout"]=="hello","quick async Bash result mismatch");
    std::future<gadgets::GadgetResult> foreign;
    std::thread caller([&]{foreign=gadget.call(Json{{"command","printf 'foreign\\n'"}},
      context,{},Json(),{});});
    caller.join();
    check(foreign.wait_for(std::chrono::milliseconds(0))==std::future_status::timeout,
      "foreign Tool.call ran before owner turn");
    loop.run();check(foreign.get().data["stdout"]=="foreign",
      "foreign Tool.call did not post to owner");
    auto large=gadget.call(Json{{"command","printf '%0200d' 0"}},context,{},Json(),{});
    loop.run();const auto largeResult=large.get();
    check(largeResult.data.contains("persistedOutputPath")&&
      largeResult.data["persistedOutputSize"]==200&&
      std::filesystem::exists(largeResult.data["persistedOutputPath"].get<std::string>()),
      "async Bash output persistence failed");
    auto failing=gadget.call(Json{{"command","exit 7"}},context,{},Json(),{});
    loop.run();bool shellError=false;
    try{(void)failing.get();}
    catch(const ShellError& error){shellError=error.code==7;}
    check(shellError,"async Bash ShellError did not reach Tool.call future");
    auto background=gadget.call(Json{{"command","sleep 0.6; printf 'late\\n'"},
      {"run_in_background",true}},context,{},Json(),{});
    bool settledWhileRunning=false;
    loop.setTimeout(120,[&]{
      if(background.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
      const auto missions=roster->list();
      settledWhileRunning=missions.size()==1&&missions.front().status=="running";
    });
    loop.run();
    const auto result=background.get();
    check(settledWhileRunning,"background Tool.call future waited for child exit");
    const auto id=result.data["backgroundTaskId"].get<std::string>();
    const auto mission=roster->get(id);
    check(mission&&mission->status=="completed","background child did not complete");
    {std::lock_guard lock(notificationMutex);
      check(!notifications.empty(),"background completion notification missing");}
    check(result.data["stdout"]=="","background tool result should be empty");
    std::stop_source stop;
    gadgets::GadgetCallContext cancelled{Json::object(),stop.get_token(),
      []{return std::string("cancel");},nullptr};
    auto cancellation=gadget.call(Json{{"command","sleep 20"}},cancelled,{},Json(),{});
    loop.setTimeout(100,[&]{stop.request_stop();});
    loop.run();bool interrupted=false;
    try{(void)cancellation.get();}
    catch(const ShellError& error){interrupted=error.interrupted&&error.code==137;}
    check(interrupted,"async Bash stop token did not cancel process group");
    check(recordedHints.empty(),"unexpected local hints");
  }catch(...){std::filesystem::remove_all(root);throw;}
  std::filesystem::remove_all(root);
}
