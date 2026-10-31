#include "gadgets/bash/AsyncBashCall.hpp"
#include "gadgets/bash/BashCompletion.hpp"
#include <chrono>
#include <stdexcept>
#include <utility>

namespace batbox::bash_gadget {
namespace {
std::string stringField(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value[key].is_string()?value[key].get<std::string>():"";
}
bool boolField(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value[key].is_boolean()&&value[key].get<bool>();
}
void require(bool value,const char* name){
  if(!value)throw std::invalid_argument(std::string("async Bash import required: ")+name);
}
shell::RunOptions optionsFor(const AsyncExecRequest& request,const Json& input,
  const gadgets::GadgetCallContext& context,std::shared_ptr<const Services> service){
  const auto& services=*service;
  shell::RunOptions options;
  options.command=request.command;
  options.description=stringField(input,"description");
  options.timeoutMs=request.timeoutMs;
  options.runInBackground=false;
  options.preventCwdChanges=request.preventCwdChanges;
  options.isMainThread=stringField(context.data,"agentId").empty();
  options.signal=context.stopToken;
  options.currentAbortReason=context.currentAbortReason?
    context.currentAbortReason:[data=context.data]{return stringField(data,"abortReason");};
  options.autoBackgroundEnabled=request.shouldAutoBackground;
  options.managedAssistantBackgroundEnabled=false;
  if(request.useSandbox){
    require(bool(services.getSandboxTmpDir),"getSandboxTmpDir");
    require(bool(services.logDebug),"logDebug");
    options.useSandbox=true;options.sandboxTmpDir=services.getSandboxTmpDir();
    options.sandboxWrap=[service,rawInput=input,data=context.data](
      const std::string& command){
      return service->sandboxCommand(command,rawInput,data);};
    options.logDebug=services.logDebug;
  }
  return options;
}
struct State:std::enable_shared_from_this<State>{
  event_loop::Loop& loop;
  std::shared_ptr<const Services> services;
  std::shared_ptr<const AsyncMissionHost> host;
  Json input;
  gadgets::GadgetCallContext context;
  std::function<void(const gadgets::GadgetProgress&)> onProgress;
  event_loop::Deferred<gadgets::GadgetResult> output;
  std::unique_ptr<AsyncRunShellCommand> generator;
  std::size_t progressCounter=0;
  bool settled=false;
  State(event_loop::Loop& owner,std::shared_ptr<const Services> service,
    std::shared_ptr<const AsyncMissionHost> missionHost,Json value,
    gadgets::GadgetCallContext gadgetContext,
    std::function<void(const gadgets::GadgetProgress&)> progress):
    loop(owner),services(std::move(service)),host(std::move(missionHost)),
    input(std::move(value)),
    context(std::move(gadgetContext)),onProgress(std::move(progress)),output(owner){}
  void reject(std::exception_ptr error,bool clearUI){
    if(settled)return;
    settled=true;
    if(clearUI&&services->clearGadgetJSX){
      try{services->clearGadgetJSX(context.data);}catch(...){error=std::current_exception();}
    }
    output.reject(error);
  }
  void resolve(gadgets::GadgetResult value){
    if(settled)return;
    settled=true;output.resolve(std::move(value));
  }
  void start(){
    try{
      AsyncRunInput command;
      command.command=stringField(input,"command");
      command.description=stringField(input,"description");
      command.runInBackground=boolField(input,"run_in_background");
      command.preventCwdChanges=!stringField(context.data,"agentId").empty();
      if(input.contains("timeout")&&input["timeout"].is_number())
        command.timeoutMs=input["timeout"].get<double>();
      if(context.data.is_object()){
        if(context.data.contains("toolUseId")&&context.data["toolUseId"].is_string())
          command.gadgetUseId=context.data["toolUseId"].get<std::string>();
        if(context.data.contains("agentId")&&context.data["agentId"].is_string())
          command.batId=context.data["agentId"].get<std::string>();
      }
      RunShellSettings settings;
      settings.backgroundMissionsDisabled=services->backgroundMissionsDisabled;
      settings.assistantMode=services->assistantAutoBackgroundEnabled;
      settings.isMainThread=!command.preventCwdChanges;
      settings.showBackgroundUI=bool(services->showBackgroundHint);
      settings.progressThresholdMs=host->getProgressThresholdMs();
      settings.assistantBlockingBudgetMs=services->assistantBlockingBudgetMs;
      AsyncRunImports imports;
      imports.exec=[&owner=loop,service=services,rawInput=input,
        gadgetContext=context](const AsyncExecRequest& request){
        return execManagedOnLoop(owner,service->shell,
          optionsFor(request,rawInput,gadgetContext,service),request.shouldAutoBackground);};
      imports.spawnBackgroundMission=[missionHost=host,gadgetContext=context](const AsyncRunInput& input,
        const AsyncShellCommand& command){
        return missionHost->spawnShellMission(input,command,gadgetContext);};
      imports.backgroundExistingForegroundMission=[missionHost=host,gadgetContext=context](const std::string& id,
        const AsyncRunInput& input,const AsyncShellCommand& command){
        return missionHost->backgroundExistingForegroundMission(id,input,command,gadgetContext);};
      imports.registerForeground=[missionHost=host,gadgetContext=context](const AsyncRunInput& input,
        const AsyncShellCommand& command){
        return missionHost->registerForeground(input,command,gadgetContext);};
      imports.unregisterForeground=[missionHost=host,gadgetContext=context](const std::string& id){
        missionHost->unregisterForeground(id,gadgetContext);};
      imports.markMissionNotified=[missionHost=host,gadgetContext=context](const std::string& id){
        missionHost->markMissionNotified(id,gadgetContext);};
      if(services->showBackgroundHint)
        imports.showBackgroundHint=[service=services,data=context.data]{
          service->showBackgroundHint(data);};
      imports.startPolling=bindMissionOutputPoller(loop,*services->missionOutputPoller);
      imports.isAutoBackgroundAllowed=host->isAutoBackgroundAllowed;
      imports.shouldUseSandbox=[service=services,rawInput=input](const AsyncRunInput&){
        return service->shouldUseSandbox(rawInput);};
      imports.nowMs=[started=std::chrono::steady_clock::now()]{
        return std::chrono::duration<double,std::milli>(
          std::chrono::steady_clock::now()-started).count();};
      imports.getDefaultTimeoutMs=host->getDefaultTimeoutMs;
      imports.onUnobservedError=host->onUnobservedError;
      generator=std::make_unique<AsyncRunShellCommand>(loop,std::move(command),
        std::move(settings),std::move(imports));
      requestNext();
    }catch(...){reject(std::current_exception(),true);}
  }
  void requestNext(){
    try{
      generator->next().then([self=shared_from_this()](const AsyncRunStep* step,
        const std::exception_ptr& error){
        if(error){self->reject(error,true);return;}
        try{
          if(step->done){
            if(!step->result)throw std::logic_error("Bash generator returned no result");
            self->afterResult(*step->result);return;
          }
          if(self->onProgress){
            auto data=step->progress->toJson();data["type"]="bash_progress";
            self->onProgress({"bash-progress-"+
              std::to_string(self->progressCounter++),std::move(data)});
          }
          self->requestNext();
        }catch(...){self->reject(std::current_exception(),true);}
      });
    }catch(...){reject(std::current_exception(),true);}
  }
  void afterResult(shell::ExecResult result){
    BashPreparedResult prepared;
    try{prepared=prepareBashCompletion(input,context,std::move(result),*services,
      host->getGadgetPermissionContext);}
    catch(...){reject(std::current_exception(),false);return;}
    if(!prepared.command.outputFilePath||prepared.command.outputFilePath->empty()||
      !prepared.command.outputMissionId||prepared.command.outputMissionId->empty()){
      afterPersistence(std::move(prepared),{});
      return;
    }
    try{
      auto self=shared_from_this();
      auto command=prepared.command;
      auto complete=[self,prepared=std::move(prepared)](
        const BashPersistedOutput& persisted){
          self->afterPersistence(prepared,persisted);
        };
      (void)persistBashOutputAsync(loop,command,host->fileOps,std::move(complete));
    }catch(...){reject(std::current_exception(),false);}
  }
  void afterPersistence(BashPreparedResult prepared,BashPersistedOutput persisted){
    try{
      auto finalInput=prepareBashFinalInput(input,prepared,*services);
      if(!finalInput.isImage){
        resolve(finishBashFinalOutput(input,prepared,persisted,finalInput,{},*services));
        return;
      }
      require(bool(host->resizeShellImageOutput),"resizeShellImageOutput");
      host->resizeShellImageOutput(finalInput.cleanOutput,
        prepared.command.outputFilePath,persisted.originalSize).then(
          [self=shared_from_this(),prepared=std::move(prepared),
            persisted=std::move(persisted),finalInput=std::move(finalInput)](
            const std::optional<std::string>* resized,const std::exception_ptr& error){
            if(error){self->reject(error,false);return;}
            try{self->resolve(finishBashFinalOutput(self->input,prepared,persisted,
              finalInput,*resized,*self->services));}
            catch(...){self->reject(std::current_exception(),false);}
          });
    }catch(...){reject(std::current_exception(),false);}
  }
};
}
event_loop::Deferred<gadgets::GadgetResult> callAsyncBash(event_loop::Loop& loop,
  std::shared_ptr<const Services> services,std::shared_ptr<const AsyncMissionHost> host,
  Json input,gadgets::GadgetCallContext context,Json parentMessage,
  std::function<void(const gadgets::GadgetProgress&)> onProgress){
  if(!loop.isOwnerThread())throw std::logic_error("async Bash call requires loop owner");
  require(bool(services),"services");
  const bool simulated=input.is_object()&&input.contains("_simulatedSedEdit")&&
    value::truthy(input.at("_simulatedSedEdit"));
  if(simulated){
    event_loop::Deferred<gadgets::GadgetResult> failed(loop);
    try{
      require(bool(host),"taskHost");require(bool(host->applySedEdit),"applySedEdit");
      return event_loop::adoptAsyncReturn(host->applySedEdit(
        input.at("_simulatedSedEdit"),context,parentMessage));
    }catch(...){failed.reject(std::current_exception());return failed;}
  }
  require(bool(host),"taskHost");
  require(bool(services->shell),"shell");require(bool(services->missionOutputPoller),"taskOutputPoller");
  require(bool(services->shouldUseSandbox),"shouldUseSandbox");
  require(bool(services->sandboxCommand),"sandboxCommand");
  require(bool(services->annotateSandboxFailures),"annotateSandboxFailures");
  require(bool(services->resetCwdIfOutsideProject),"resetCwdIfOutsideProject");
  require(bool(services->splitCommand),"splitCommand");
  require(bool(services->splitWithOperators),"splitWithOperators");
  require(bool(services->extractHints),"extractHints");
  require(bool(services->recordHint),"recordHint");
  require(bool(services->getGadgetResultPath),"getToolResultPath");
  require(bool(host->spawnShellMission),"spawnShellTask");
  require(bool(host->backgroundExistingForegroundMission),"backgroundExistingForegroundTask");
  require(bool(host->registerForeground),"registerForeground");
  require(bool(host->unregisterForeground),"unregisterForeground");
  require(bool(host->markMissionNotified),"markTaskNotified");
  require(bool(host->isAutoBackgroundAllowed),"isAutoBackgroundAllowed");
  require(bool(host->getGadgetPermissionContext),"getToolPermissionContext");
  require(bool(host->getDefaultTimeoutMs),"getDefaultTimeoutMs");
  require(bool(host->getProgressThresholdMs),"getProgressThresholdMs");
  require(bool(host->onUnobservedError),"onUnobservedError");
  auto state=std::make_shared<State>(loop,std::move(services),std::move(host),
    std::move(input),std::move(context),std::move(onProgress));
  auto output=state->output;state->start();return output;
}
} // namespace batbox::bash_gadget
