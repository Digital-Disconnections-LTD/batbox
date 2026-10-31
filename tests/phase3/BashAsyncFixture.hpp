#pragma once
#include "gadgets/bash/AsyncBashCall.hpp"
#include "shell/CommandSplitter.hpp"
#include "missions/shell/Roster.hpp"
#include <atomic>
#include <filesystem>
#include <future>
#include <memory>
#include <stdexcept>

namespace batbox::test_support {
template<class T>std::future<T> readyFuture(T value){
  std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();
}
// Test-only real shell/mission setup. Keep this object alive through background
// process completion; callbacks capture its stable address.
struct BashAsyncFixture final {
  event_loop::Loop& loop;
  std::filesystem::path root;
  std::atomic<int> ids{0};
  std::shared_ptr<shell_missions::Roster> roster;
  std::shared_ptr<shell::ManagedShell> shell;
  gadgets::Gadget gadget;
  static std::shared_ptr<BashAsyncFixture> create(event_loop::Loop& loop,
    std::filesystem::path root){
    auto fixture=std::shared_ptr<BashAsyncFixture>(new BashAsyncFixture(loop,std::move(root)));
    fixture->bind();return fixture;
  }
private:
  BashAsyncFixture(event_loop::Loop& owner,std::filesystem::path path):
    loop(owner),root(std::move(path)){}
  void bind(){
    std::filesystem::create_directories(root);
    shell_missions::Services missionServices;
    missionServices.nowMs=[] {return 1234LL;};
    missionServices.enqueueNotification=[](const std::string&,
      const std::optional<std::string>&){};
    missionServices.onMissionTerminated=[](const shell_missions::MissionRecord&){};
    roster=std::make_shared<shell_missions::Roster>(missionServices);
    shell::FlightConfig config;config.shellPath="/bin/bash";
    config.originalCwd=root.string();config.outputDir=(root/"jobs").string();
    config.maxInlineOutputBytes=100;
    config.generateMissionId=[this]{return "async-task-"+std::to_string(++ids);};
    config.splitCommand=[](const std::string& command){
      return shell_split::splitCommandWithOperators(command,
        []{return std::string("async-fixture-salt");});};
    config.onBackgroundComplete=[weak=std::weak_ptr(roster)](
      const std::string& id,const shell::ExecResult& result){
      if(auto missions=weak.lock())missions->completeShell(id,result);};
    shell=std::make_shared<shell::ManagedShell>(std::move(config));
    roster->attachShell(shell);
    bash_gadget::Services services;services.shell=shell;
    services.missionOutputPoller=std::make_shared<shell::MissionOutputPoller>();
    services.originalCwd=root.string();
    bash_gadget::bindIndigenousCommandSplitters(services);
    bash_gadget::bindIndigenousBashSchema(services,600000);
    bash_gadget::bindIndigenousHintParser(services,{"batbox-hint",[](const std::string&){}});
    services.poetry=[](const gadgets::PoetryContext&){return readyFuture(std::string("Run shell commands"));};
    services.checkPermissions=[](const Json& input,const Json&){
      return readyFuture(Json{{"behavior","allow"},{"updatedInput",input}});};
    services.readOnlyAllowed=[](const Json&,bool hasCd){return !hasCd;};
    services.commandHasAnyCd=[](const std::string& command){
      return command.find("cd ")!=std::string::npos;};
    services.preparePermissionMatcher=[](const Json&){
      return readyFuture(std::function<bool(const std::string&)>(
        [](const std::string&){return true;}));};
    services.shouldUseSandbox=[](const Json&){return false;};
    services.sandboxCommand=[](const std::string& command,const Json&,const Json&){return command;};
    services.annotateSandboxFailures=[](const std::string&,const std::string& output){return output;};
    services.resetCwdIfOutsideProject=[](const Json&){return false;};
    services.recordHint=[](const Json&){};
    services.resizeImage=[](const std::string&,const shell::ExecResult&)->std::optional<std::string>{
      throw std::runtime_error("fixture image resize not bound");};
    services.applySedEdit=[](const Json&,const gadgets::GadgetCallContext&,
      const Json&)->gadgets::GadgetResult{throw std::runtime_error("fixture sed edit not bound");};
    services.parseSedEdit=[](const std::string&){return Json();};
    services.fileEditUserFacingName=[](const Json&){return std::string("Edit");};
    services.truncateSummary=[](const std::string& value,std::size_t max){return value.substr(0,max);};
    services.getGadgetResultPath=[path=root](const std::string& id){
      return (path/"results"/(id+".txt")).string();};
    services.registerBackgroundMission=[missions=roster](const std::string& id,
      const std::string& command,const std::string& description,const Json&){
      shell_missions::MissionRecord record;record.id=id;record.command=command;
      record.description=description;missions->adoptBackground(std::move(record));};
    services.registerForegroundMission=[missions=roster](const std::string& id,
      const std::string& command,const std::string& description,const Json&){
      shell_missions::MissionRecord record;record.id=id;record.command=command;
      record.description=description;missions->registerForeground(std::move(record));};
    services.unregisterForegroundMission=[missions=roster](const std::string& id){
      (void)missions->unregisterForeground(id);};
    services.backgroundExistingForegroundMission=[missions=roster](const std::string& id){
      return missions->backgroundExistingForeground(id);};
    services.markMissionNotified=[missions=roster](const std::string& id){
      (void)missions->markNotified(id);};
    services.renderUse=[](const Json&,const Json&){return Json("use");};
    services.renderResult=[](const Json&,const Json&,const Json&){return Json("result");};
    services.renderProgress=[](const Json&,const Json&){return Json("progress");};
    services.renderQueued=[](){return Json("queued");};
    services.renderError=[](const Json&,const Json&){return Json("error");};
    bash_gadget::AsyncMissionHost host;
    host.applySedEdit=[this](const Json&,const gadgets::GadgetCallContext&,const Json&){
      event_loop::Deferred<gadgets::GadgetResult> output(loop);
      output.reject(std::make_exception_ptr(std::runtime_error("fixture sed edit not bound")));
      return output;};
    host.spawnShellMission=[this](const bash_gadget::AsyncRunInput& input,
      const bash_gadget::AsyncShellCommand& command,const gadgets::GadgetCallContext&){
      shell_missions::MissionRecord record;record.id=command.missionId;record.command=input.command;
      record.description=input.description.empty()?input.command:input.description;
      record.gadgetUseId=input.gadgetUseId;record.batId=input.batId;
      roster->adoptBackground(std::move(record));
      if(!command.background(command.missionId))throw std::runtime_error("background transition failed");
      event_loop::Deferred<std::string> done(loop);done.resolve(command.missionId);return done;};
    host.backgroundExistingForegroundMission=[missions=roster](const std::string& id,
      const bash_gadget::AsyncRunInput&,const bash_gadget::AsyncShellCommand&,
      const gadgets::GadgetCallContext&){return missions->backgroundExistingForeground(id);};
    host.registerForeground=[missions=roster](const bash_gadget::AsyncRunInput& input,
      const bash_gadget::AsyncShellCommand& command,const gadgets::GadgetCallContext&){
      shell_missions::MissionRecord record;record.id=command.missionId;record.command=input.command;
      record.description=input.description.empty()?input.command:input.description;
      missions->registerForeground(std::move(record));return command.missionId;};
    host.unregisterForeground=[missions=roster](const std::string& id,
      const gadgets::GadgetCallContext&){(void)missions->unregisterForeground(id);};
    host.markMissionNotified=[missions=roster](const std::string& id,
      const gadgets::GadgetCallContext&){(void)missions->markNotified(id);};
    auto splitter=services.splitCommand;
    host.isAutoBackgroundAllowed=[splitter](const std::string& command){
      return shell::isAutobackgroundingAllowed(command,splitter);};
    host.getDefaultTimeoutMs=[] {return 5000.0;};
    host.getProgressThresholdMs=[] {return 20.0;};
    host.getGadgetPermissionContext=[] {return Json::object();};
    host.onUnobservedError=[](std::exception_ptr error){std::rethrow_exception(error);};
    host.fileOps=bash_gadget::indigenousBashFileOps(loop,(root/"results").string(),
      [path=root](const std::string& id,bool){return (path/"results"/(id+".txt")).string();});
    host.resizeShellImageOutput=[this](const std::string&,
      const std::optional<std::string>&,const std::optional<std::uintmax_t>&){
      event_loop::Deferred<std::optional<std::string>> output(loop);
      output.reject(std::make_exception_ptr(std::runtime_error("fixture image resize not bound")));
      return output;};
    gadget=bash_gadget::makeAsyncBashGadget(std::move(services),loop,std::move(host));
  }
};
} // namespace batbox::test_support
