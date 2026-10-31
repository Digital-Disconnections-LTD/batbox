#include "application_shell/ApplicationShell.hpp"
#include "shell/CommandSplitter.hpp"
#include "runtime/JsonValue.hpp"
#include <regex>
#include <stdexcept>
#include <utility>

namespace batbox::application_shell {
namespace {
template<class F>void need(const F& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Application shell import required: ")+name);
}
std::string field(const Json& value,const char* name){
  return value.is_object()&&value.contains(name)&&value.at(name).is_string()
    ?value.at(name).get<std::string>():"";
}
shell_missions::MissionRecord record(const std::string& id,
    const std::string& command,const std::string& description,const Json& context){
  shell_missions::MissionRecord value;
  value.id=id;value.command=command;value.description=description;
  const auto gadgetUseId=field(context,"toolUseId");
  if(!gadgetUseId.empty())value.gadgetUseId=gadgetUseId;
  const auto batId=field(context,"agentId");
  if(!batId.empty())value.batId=batId;
  return value;
}
}

Host makeHost(Inputs input){
  if(!input.owner)throw std::invalid_argument("Application shell owner loop required");
  if(input.shellPath.empty()||input.originalCwd.empty()||input.outputDir.empty())
    throw std::invalid_argument("Application shell paths required");
  need(input.generateMissionId,"task ID generator");
  need(input.nowMs,"clock");
  need(input.enqueueNotification,"background notification queue");
  need(input.onMissionTerminated,"task termination observer");
  if(input.provider)need(input.generateExecId,"shell execution ID generator");
  const auto dispatcher=input.owner->dispatcher();
  shell_missions::Services missionServices;
  missionServices.nowMs=input.nowMs;
  missionServices.enqueueNotification=[dispatcher,callback=input.enqueueNotification](
      const std::string& message,const std::optional<std::string>& batId){
    dispatcher.postMission([callback,message,batId]{callback(message,batId);});
  };
  missionServices.onMissionTerminated=[dispatcher,callback=input.onMissionTerminated](
      const shell_missions::MissionRecord& mission){
    dispatcher.postMission([callback,mission]{callback(mission);});
  };
  auto missions=std::make_shared<shell_missions::Roster>(std::move(missionServices));
  shell::FlightConfig config;
  config.shellPath=std::move(input.shellPath);
  config.originalCwd=std::move(input.originalCwd);
  config.outputDir=std::move(input.outputDir);
  config.provider=std::move(input.provider);
  config.generateMissionId=std::move(input.generateMissionId);
  config.generateExecId=std::move(input.generateExecId);
  config.splitCommand=[](const std::string& command){
    return shell_split::splitCommandDeprecated(command);};
  config.maxInlineOutputBytes=input.maxInlineOutputBytes;
  config.defaultTimeoutMs=input.defaultTimeoutMs;
  config.maxTimeoutMs=input.maxTimeoutMs;
  config.backgroundMissionsDisabled=input.backgroundMissionsDisabled;
  config.assistantAutoBackgroundEnabled=input.assistantAutoBackgroundEnabled;
  config.assistantBlockingBudgetMs=input.assistantBlockingBudgetMs;
  config.cwdCompletion=std::move(input.cwdCompletion);
  const auto weak=std::weak_ptr(missions);
  config.onBackgroundComplete=[weak](const std::string& id,const shell::ExecResult& result){
    if(auto roster=weak.lock())roster->completeShell(id,result);};
  config.onBackgroundError=[dispatcher,callback=std::move(input.onBackgroundError)](
      const std::string&,std::exception_ptr error){
    if(callback)dispatcher.postMission([callback,error]{callback(error);});};
  auto shell=std::make_shared<shell::ManagedShell>(std::move(config));
  missions->attachShell(shell);
  return {std::move(shell),std::make_shared<shell::MissionOutputPoller>(),
    std::move(missions)};
}

void bindBashMissionServices(bash_gadget::Services& services,const Host& host,
    const gadget_results::StorageImports& storage,
    std::function<void(const Json&)> showBackgroundHint){
  if(!host.shell||!host.poller||!host.missions)
    throw std::invalid_argument("Application shell host incomplete");
  need(storage.getOriginalCwd,"tool-result original cwd");
  need(storage.getProjectDir,"tool-result project directory");
  need(storage.getFlightId,"tool-result session ID");
  services.shell=host.shell;services.missionOutputPoller=host.poller;
  services.originalCwd=storage.getOriginalCwd();
  auto missions=host.missions;
  services.registerBackgroundMission=[missions](const std::string& id,
      const std::string& command,const std::string& description,const Json& context){
    missions->adoptBackground(record(id,command,description,context));};
  services.registerForegroundMission=[missions](const std::string& id,
      const std::string& command,const std::string& description,const Json& context){
    missions->registerForeground(record(id,command,description,context));};
  services.unregisterForegroundMission=[missions](const std::string& id){
    (void)missions->unregisterForeground(id);};
  services.backgroundExistingForegroundMission=[missions](const std::string& id){
    return missions->backgroundExistingForeground(id);};
  services.markMissionNotified=[missions](const std::string& id){
    (void)missions->markNotified(id);};
  if(showBackgroundHint)services.showBackgroundHint=std::move(showBackgroundHint);
  services.getGadgetResultPath=[storage](const std::string& id){
    return gadget_results::getGadgetResultPath(storage,id,false);};
}
void bindConservativeBashClassification(bash_gadget::Services& services){
  if(!services.commandHasAnyCd)services.commandHasAnyCd=[](const std::string& command){
    // Detect direct directory-changing commands for shell classification.
    // Nested wrappers are not normalized here.
    static const std::regex directoryChange(
      R"((^|[[:space:];&|()])(cd|pushd|popd)([[:space:];&|()]|$))");
    for(const auto& part:shell_split::splitCommandDeprecated(command)){
      const auto trimmed=value::trim(part);
      if(std::regex_search(trimmed,directoryChange))return true;
    }
    return false;
  };
  if(!services.readOnlyAllowed)services.readOnlyAllowed=[](const Json&,bool){
    return false;
  };
}
} // namespace batbox::application_shell
