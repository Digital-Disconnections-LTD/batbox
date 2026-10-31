#include <cstdlib>
#include "gadgets/bash/AsyncBashCall.hpp"
#include "shell/CommandSplitter.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <unistd.h>

using namespace batbox;
using namespace batbox::bash_gadget;
namespace {
template<class T>T ready(T value){std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future().get();}
template<class T>std::future<T> readyFuture(T value){std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();}
Json evaluate(const Json& item,const std::filesystem::path& directory){
  std::filesystem::create_directories(directory);
  event_loop::Loop loop;
  shell::FlightConfig config;config.shellPath="/bin/bash";config.originalCwd=directory.string();
  config.outputDir=(directory/"jobs").string();config.maxInlineOutputBytes=10000;
  config.generateMissionId=[](){return std::string("task1");};
  config.splitCommand=[](const std::string& command){return shell_split::splitCommandDeprecated(command);};
  auto shell=std::make_shared<shell::ManagedShell>(config);
  Services services;services.shell=shell;services.originalCwd=directory.string();
  services.missionOutputPoller=std::make_shared<shell::MissionOutputPoller>();
  bindIndigenousCommandSplitters(services);
  bindIndigenousBashSchema(services,600000);
  bindIndigenousHintParser(services,{"batbox-hint",[](const std::string&){}});
  services.poetry=[](const gadgets::PoetryContext&){return readyFuture(std::string("Bash"));};
  services.checkPermissions=[](const Json& input,const Json&){return readyFuture(Json{{"behavior","allow"},{"updatedInput",input}});};
  services.readOnlyAllowed=[](const Json&,bool){return false;};
  services.commandHasAnyCd=[](const std::string&){return false;};
  services.preparePermissionMatcher=[](const Json&){return readyFuture(
    std::function<bool(const std::string&)>([](const std::string&){return true;}));};
  services.shouldUseSandbox=[](const Json&){return false;};
  services.sandboxCommand=[](const std::string& command,const Json&,const Json&){return command;};
  services.annotateSandboxFailures=[](const std::string&,const std::string& output){return output;};
  services.resetCwdIfOutsideProject=[](const Json&){return false;};
  services.resizeImage=[](const std::string&,const shell::ExecResult&){return std::optional<std::string>{};};
  services.parseSedEdit=[](const std::string&){return Json();};
  services.fileEditUserFacingName=[](const Json&){return std::string("Edit");};
  services.truncateSummary=[](const std::string& value,std::size_t){return value;};
  services.getGadgetResultPath=[directory](const std::string& id){return (directory/(id+".txt")).string();};
  std::optional<std::string> backgroundMissionId;
  services.registerBackgroundMission=[&](const std::string& id,const std::string&,const std::string&,const Json&){
    backgroundMissionId=id;};
  services.registerForegroundMission=[](const std::string&,const std::string&,
    const std::string&,const Json&){};
  services.unregisterForegroundMission=[](const std::string&){};
  services.backgroundExistingForegroundMission=[](const std::string&){return false;};
  services.markMissionNotified=[](const std::string&){};
  services.renderUse=[](const Json&,const Json&){return Json();};
  services.renderResult=[](const Json&,const Json&,const Json&){return Json();};
  services.renderProgress=[](const Json&,const Json&){return Json();};
  services.renderQueued=[](){return Json();};
  services.renderError=[](const Json&,const Json&){return Json();};
  Json hints=Json::array(),ui=Json::array(),progress=Json::array(),events=Json::array();
  services.recordHint=[&](const Json& hint){hints.push_back(hint);};
  services.clearGadgetJSX=[&](const Json&){ui.push_back(nullptr);};
  services.applySedEdit=[&](const Json&,const gadgets::GadgetCallContext&,const Json&){
    gadgets::GadgetResult result;result.data=item.at("sedResult");return result;};
  AsyncMissionHost host;
  host.applySedEdit=[&](const Json&,const gadgets::GadgetCallContext&,const Json&){
    event_loop::Deferred<gadgets::GadgetResult> ready(loop);
    gadgets::GadgetResult result;result.data=item.at("sedResult");
    ready.resolve(std::move(result));return ready;
  };
  host.spawnShellMission=[&](const AsyncRunInput&,
    const AsyncShellCommand& command,const gadgets::GadgetCallContext&){
    backgroundMissionId=command.missionId;
    (void)command.background(command.missionId);
    event_loop::Deferred<std::string> ready(loop);ready.resolve(command.missionId);return ready;};
  host.backgroundExistingForegroundMission=[](const std::string&,
    const AsyncRunInput&,const AsyncShellCommand&,const gadgets::GadgetCallContext&){return false;};
  host.registerForeground=[](const AsyncRunInput&,const AsyncShellCommand& command,
    const gadgets::GadgetCallContext&){return command.missionId;};
  host.unregisterForeground=[](const std::string&,const gadgets::GadgetCallContext&){};
  host.markMissionNotified=[](const std::string&,const gadgets::GadgetCallContext&){};
  auto splitter=services.splitCommand;
  host.isAutoBackgroundAllowed=[splitter](const std::string& command){
    return shell::isAutobackgroundingAllowed(command,splitter);};
  host.getDefaultTimeoutMs=[] {return 5000.0;};
  host.getProgressThresholdMs=[] {return 2000.0;};
  host.getGadgetPermissionContext=[] {return Json::object();};
  host.onUnobservedError=[](std::exception_ptr error){std::rethrow_exception(error);};
  host.resizeShellImageOutput=[&](const std::string&,
    const std::optional<std::string>&,const std::optional<std::uintmax_t>&){
    event_loop::Deferred<std::optional<std::string>> ready(loop);
    if(item.value("traceImage",false)){
      events.push_back("resize-enter");
      loop.queueMicrotask([&events]{events.push_back("resize-sibling");});
    }
    loop.queueMicrotask([ready,&item,&events]{
      if(item.value("traceImage",false))events.push_back("resize-settle");
      if(item.contains("resizeError")&&item["resizeError"].is_string())
        ready.reject(std::make_exception_ptr(std::runtime_error(
          item["resizeError"].get<std::string>())));
      else if(item.contains("resizeResult")&&item["resizeResult"].is_string())
        ready.resolve(item["resizeResult"].get<std::string>());
      else ready.resolve(std::nullopt);
    });
    return ready;
  };
  const bool traceImage=item.value("traceImage",false);
  std::shared_ptr<Services> servicePtr;
  std::shared_ptr<AsyncMissionHost> hostPtr;
  if(traceImage){servicePtr=std::make_shared<Services>(services);hostPtr=std::make_shared<AsyncMissionHost>(host);}
  const auto gadget=traceImage?gadgets::Gadget{}:makeAsyncBashGadget(std::move(services),loop,std::move(host));
  gadgets::GadgetCallContext context;context.data=Json::object();
  Json record;
  try{
    auto onProgress=[&](const gadgets::GadgetProgress& event){progress.push_back({{"toolUseID",event.gadgetUseID},
      {"data",event.data}});};
    if(traceImage){
      auto pending=callAsyncBash(loop,servicePtr,hostPtr,item.at("input"),context,Json(),onProgress);
      pending.then([&](const gadgets::GadgetResult* value,const std::exception_ptr& error){
        if(error){events.push_back("outer-error");try{std::rethrow_exception(error);}catch(const std::exception& e){
          record["error"]={{"name","Error"},{"message",e.what()}};}}
        else {events.push_back("outer");record["value"]={{"data",value->data}};}
      });
      loop.run();
    }else{
      auto pending=gadget.call(item.at("input"),context,{},Json(),onProgress);
      loop.run();const auto result=pending.get();record["value"]={{"data",result.data}};
    }
  }catch(const ShellError& error){
    record["error"]={{"name","ShellError"},{"message",error.what()},
      {"stdout",error.stdoutText},{"stderr",error.stderrText},{"code",error.code},
      {"interrupted",error.interrupted}};
  }catch(const std::exception& error){
    record["error"]={{"name","Error"},{"message",error.what()}};
  }
  if(backgroundMissionId){(void)shell->wait(*backgroundMissionId,3000);shell->cleanup(*backgroundMissionId);}
  record["progress"]=progress;record["hints"]=hints;record["ui"]=ui;
  if(traceImage)record["events"]=events;
  return record;
}
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);const std::string data(std::istreambuf_iterator<char>{file},{});
  const auto cases=value::parse(data);
  char temp[]="/tmp/batbox-bash-call-XXXXXX";if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=temp;
  try{
    Json output=Json::array();std::size_t index=0;
    for(const auto& item:cases)output.push_back(evaluate(item,root/std::to_string(index++)));
    std::cout<<value::stringify(output)<<'\n';std::filesystem::remove_all(root);return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
