#include "application_shell/ApplicationShell.hpp"
#include "shell/CommandSplitter.hpp"
#include "gadgets/bash/AsyncBashImage.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::application_shell {
namespace {
template<class F>void need(const F& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Application async Bash import required: ")+name);
}
shell_missions::MissionRecord record(const bash_gadget::AsyncRunInput& input,
    const bash_gadget::AsyncShellCommand& command){
  shell_missions::MissionRecord mission;
  mission.id=command.missionId;
  mission.command=input.command;
  mission.description=input.description.empty()?input.command:input.description;
  mission.gadgetUseId=input.gadgetUseId;
  mission.batId=input.batId;
  return mission;
}
}

bash_gadget::AsyncMissionHost makeAsyncBashMissionHost(AsyncHostInputs inputs){
  if(!inputs.owner.isOwnerThread())
    throw std::logic_error("Application async Bash host requires owner thread");
  if(!inputs.shell.shell||!inputs.shell.poller||!inputs.shell.missions)
    throw std::invalid_argument("Application shell host incomplete");
  need(inputs.storage.getOriginalCwd,"tool-result original cwd");
  need(inputs.storage.getProjectDir,"tool-result project directory");
  need(inputs.storage.getFlightId,"tool-result session ID");
  need(inputs.getGadgetPermissionContext,"live tool permission context");
  need(inputs.applySedEdit,"simulated sed edit");
  need(inputs.onUnobservedError,"background error observer");
  if(inputs.defaultTimeoutMs<=0||inputs.progressThresholdMs<0)
    throw std::invalid_argument("Application async Bash timeouts invalid");
  auto* owner=&inputs.owner;
  auto shell=inputs.shell.shell;
  auto roster=inputs.shell.missions;
  bash_gadget::AsyncMissionHost host;
  host.applySedEdit=std::move(inputs.applySedEdit);
  host.spawnShellMission=[owner,roster](const bash_gadget::AsyncRunInput& input,
      const bash_gadget::AsyncShellCommand& command,
      const gadgets::GadgetCallContext&){
    event_loop::Deferred<std::string> ready(*owner);
    try{
      roster->adoptBackground(record(input,command));
      if(!command.background(command.missionId)){
        // The process can finish between registration and backgrounding. Its
        // result still closes the registered mission and wakes MissionOutput.
        command.result.then([roster,id=command.missionId](const shell::ExecResult* value,
            const std::exception_ptr& error){
          if(value)roster->completeShell(id,*value);
          else if(error){shell::ExecResult failed;failed.code=1;
            roster->completeShell(id,failed);}
        });
      }
      ready.resolve(command.missionId);
    }catch(...){ready.reject(std::current_exception());}
    return ready;
  };
  host.backgroundExistingForegroundMission=[roster](const std::string& id,
      const bash_gadget::AsyncRunInput&,const bash_gadget::AsyncShellCommand&,
      const gadgets::GadgetCallContext&){
    return roster->backgroundExistingForeground(id);
  };
  host.registerForeground=[roster](const bash_gadget::AsyncRunInput& input,
      const bash_gadget::AsyncShellCommand& command,const gadgets::GadgetCallContext&){
    roster->registerForeground(record(input,command));
    return command.missionId;
  };
  host.unregisterForeground=[roster](const std::string& id,
      const gadgets::GadgetCallContext&){(void)roster->unregisterForeground(id);};
  host.markMissionNotified=[roster](const std::string& id,
      const gadgets::GadgetCallContext&){(void)roster->markNotified(id);};
  host.isAutoBackgroundAllowed=[](const std::string& command){
    return shell::isAutobackgroundingAllowed(command,[](const std::string& value){
      return shell_split::splitCommandDeprecated(value);});
  };
  host.getGadgetPermissionContext=std::move(inputs.getGadgetPermissionContext);
  host.getDefaultTimeoutMs=[value=inputs.defaultTimeoutMs]{return value;};
  host.getProgressThresholdMs=[value=inputs.progressThresholdMs]{return value;};
  host.onUnobservedError=std::move(inputs.onUnobservedError);
  auto storage=std::move(inputs.storage);
  auto resultsDir=gadget_results::getGadgetResultsDir(storage);
  host.fileOps=bash_gadget::indigenousBashFileOps(*owner,std::move(resultsDir),
    [storage=std::move(storage)](const std::string& id,bool isJson){
      return gadget_results::getGadgetResultPath(storage,id,isJson);
    });
  host.resizeShellImageOutput=inputs.resizeShellImageOutput?
    std::move(inputs.resizeShellImageOutput):bash_gadget::indigenousBashImageCallback(*owner);
  return host;
}
} // namespace batbox::application_shell
