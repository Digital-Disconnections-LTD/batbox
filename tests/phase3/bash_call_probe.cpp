#include <cstdlib>
#include "gadgets/bash/BashGadget.hpp"
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
  shell::FlightConfig config;config.shellPath="/bin/bash";config.originalCwd=directory.string();
  config.outputDir=(directory/"jobs").string();config.maxInlineOutputBytes=10000;
  config.generateMissionId=[](){return std::string("task1");};
  config.splitCommand=[](const std::string& command){return shell_split::splitCommandDeprecated(command);};
  auto shell=std::make_shared<shell::ManagedShell>(config);
  Services services;services.shell=shell;services.originalCwd=directory.string();
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
  services.renderUse=[](const Json&,const Json&){return Json();};
  services.renderResult=[](const Json&,const Json&,const Json&){return Json();};
  services.renderProgress=[](const Json&,const Json&){return Json();};
  services.renderQueued=[](){return Json();};
  services.renderError=[](const Json&,const Json&){return Json();};
  Json hints=Json::array(),ui=Json::array(),progress=Json::array();
  services.recordHint=[&](const Json& hint){hints.push_back(hint);};
  services.clearGadgetJSX=[&](const Json&){ui.push_back(nullptr);};
  services.applySedEdit=[&](const Json&,const gadgets::GadgetCallContext&,const Json&){
    gadgets::GadgetResult result;result.data=item.at("sedResult");return result;};
  const auto gadget=makeBashGadget(std::move(services));
  gadgets::GadgetCallContext context;context.data=Json::object();
  Json record;
  try{
    const auto result=gadget.call(item.at("input"),context,{},Json(),
      [&](const gadgets::GadgetProgress& event){progress.push_back({{"toolUseID",event.gadgetUseID},
        {"data",event.data}});}).get();
    record["value"]={{"data",result.data}};
  }catch(const ShellError& error){
    record["error"]={{"name","ShellError"},{"message",error.what()},
      {"stdout",error.stdoutText},{"stderr",error.stderrText},{"code",error.code},
      {"interrupted",error.interrupted}};
  }
  if(backgroundMissionId){(void)shell->wait(*backgroundMissionId,3000);shell->cleanup(*backgroundMissionId);}
  record["progress"]=progress;record["hints"]=hints;record["ui"]=ui;
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
