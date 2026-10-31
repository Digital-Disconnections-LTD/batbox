#include "runtime/AsyncLaunch.hpp"
#include "gadgets/bash/BashGadget.hpp"
#include "shell/CommandSplitter.hpp"
#include "gadgets/bash/Schema.hpp"
#include "gadgets/bash/Accumulator.hpp"
#include "gadgets/bash/BashCompletion.hpp"
#include "gadgets/bash/RunShellCommand.hpp"
#include <future>

namespace batbox::bash_gadget {
ShellError::ShellError(std::string out,std::string err,int exitCode,bool stopped):
  std::runtime_error("Shell command failed"),stdoutText(std::move(out)),stderrText(std::move(err)),
  code(exitCode),interrupted(stopped){}
namespace {
std::string stringField(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value[key].is_string()?value[key].get<std::string>():"";
}
bool boolField(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value[key].is_boolean()&&value[key].get<bool>();
}
bool isOutputTruncated(const std::string& content){
  std::size_t pos=0;
  for(int i=0;i<=3;i++){pos=content.find('\n',pos);if(pos==std::string::npos)return false;++pos;}
  return pos<content.size();
}
void require(bool ready,const char* name){if(!ready)throw std::invalid_argument(std::string("Bash service required: ")+name);}
gadgets::GadgetResult callBash(const Json& input,const gadgets::GadgetCallContext& context,
  const Json& parentMessage,const std::function<void(const gadgets::GadgetProgress&)>& onProgress,
  const Services& services){
  if(input.is_object()&&input.contains("_simulatedSedEdit")&&
    value::truthy(input.at("_simulatedSedEdit")))
    return services.applySedEdit(input.at("_simulatedSedEdit"),context,parentMessage);
  const auto command=stringField(input,"command");
  const auto description=stringField(input,"description");
  const bool mainThread=stringField(context.data,"agentId").empty();
  const bool sandbox=services.shouldUseSandbox(input);
  shell::RunOptions options;
  options.command=command;
  options.description=description;
  if(sandbox){
    if(!services.getSandboxTmpDir||!services.logDebug)
      throw std::invalid_argument("sandbox temp directory and diagnostic services required");
    options.useSandbox=true;options.sandboxTmpDir=services.getSandboxTmpDir();
    options.sandboxWrap=[&services,input,context](const std::string& built){
      return services.sandboxCommand(built,input,context.data);};
    options.logDebug=services.logDebug;
  }
  if(input.contains("timeout")&&input["timeout"].is_number()){
    const double requested=input["timeout"].get<double>();
    if(requested!=0)options.timeoutMs=requested;
  }
  options.runInBackground=boolField(input,"run_in_background")&&!services.backgroundMissionsDisabled;
  options.preventCwdChanges=!mainThread;options.isMainThread=mainThread;options.signal=context.stopToken;
  options.currentAbortReason=context.currentAbortReason?context.currentAbortReason:
    [data=context.data]{return stringField(data,"abortReason");};
  const bool sourceRunner=bool(services.missionOutputPoller);
  std::size_t progressCounter=0;
  bool foregroundRegistered=false;
  std::string foregroundMissionId;
  std::exception_ptr progressFailure;
  options.onProgress=[&](const shell::Progress& progress){
    try{
      if(services.showBackgroundHint&&!services.backgroundMissionsDisabled){
        if(!foregroundRegistered){
          services.registerForegroundMission(progress.missionId,command,
            description.empty()?command:description,context.data);
          foregroundRegistered=true;
          foregroundMissionId=progress.missionId;
        }
        services.showBackgroundHint(context.data);
      }
      if(onProgress){
        auto data=progress.toJson();data["type"]="bash_progress";
        onProgress({"bash-progress-"+std::to_string(progressCounter++),data});
      }
    }catch(...){
      progressFailure=std::current_exception();
      try{services.shell->stop(progress.missionId);}catch(...){}
    }
  };
  shell::ExecResult result;
  try{
    if(sourceRunner){
      RunShellCallbacks callbacks;
      callbacks.spawnBackgroundMission=[&](const shell::CommandHandle& handle){
        services.registerBackgroundMission(handle.missionId(),command,
          description.empty()?command:description,context.data);
        (void)handle.background(handle.missionId());return handle.missionId();};
      callbacks.registerForeground=[&](const shell::CommandHandle& handle){
        services.registerForegroundMission(handle.missionId(),command,
          description.empty()?command:description,context.data);
        return handle.missionId();};
      callbacks.backgroundExistingForegroundMission=[&](const std::string& id,
        const shell::CommandHandle&){return services.backgroundExistingForegroundMission(id);};
      callbacks.unregisterForeground=services.unregisterForegroundMission;
      callbacks.markMissionNotified=services.markMissionNotified;
      callbacks.showBackgroundHint=[&]{services.showBackgroundHint(context.data);};
      callbacks.yieldProgress=[&](const shell::Progress& progress){
        if(onProgress){auto data=progress.toJson();data["type"]="bash_progress";
          onProgress({"bash-progress-"+std::to_string(progressCounter++),data});}};
      RunShellSettings settings;settings.backgroundMissionsDisabled=services.backgroundMissionsDisabled;
      settings.runInBackground=options.runInBackground;
      settings.assistantMode=services.assistantAutoBackgroundEnabled;
      settings.isMainThread=mainThread;settings.showBackgroundUI=bool(services.showBackgroundHint);
      settings.assistantBlockingBudgetMs=services.assistantBlockingBudgetMs;
      settings.explicitTimeoutMs=options.timeoutMs;
      result=runShellCommand(services.shell,*services.missionOutputPoller,options,settings,callbacks);
    }else result=services.shell->run(options);
    if(!sourceRunner){
      if(result.backgroundMissionId)services.registerBackgroundMission(*result.backgroundMissionId,
        command,description.empty()?command:description,context.data);
      else if(foregroundRegistered)services.unregisterForegroundMission(foregroundMissionId);
    }
    if(progressFailure)std::rethrow_exception(progressFailure);
  }catch(...){
    if(services.clearGadgetJSX)services.clearGadgetJSX(context.data);
    throw;
  }
  const auto prepared=prepareBashCompletion(input,context,std::move(result),services);
  const auto persisted=persistBashOutput(prepared.command,services);
  return finishBashCompletion(input,prepared,persisted,services);
}
} // namespace
gadgets::Gadget makeBashGadget(Services services){
  require(bool(services.shell),"shell");require(bool(services.splitCommand),"splitCommand");
  require(bool(services.splitWithOperators),"splitWithOperators");
  require(bool(services.parseInput),"parseInput");require(bool(services.poetry),"prompt");
  require(!services.inputJSONSchema.is_null(),"inputJSONSchema");
  require(!services.outputJSONSchema.is_null(),"outputJSONSchema");
  require(bool(services.checkPermissions),"checkPermissions");
  require(bool(services.readOnlyAllowed),"readOnlyAllowed");
  require(bool(services.commandHasAnyCd),"commandHasAnyCd");
  require(bool(services.preparePermissionMatcher),"preparePermissionMatcher");
  require(bool(services.shouldUseSandbox),"shouldUseSandbox");
  require(bool(services.sandboxCommand),"sandboxCommand");
  require(bool(services.annotateSandboxFailures),"annotateSandboxFailures");
  require(bool(services.resetCwdIfOutsideProject),"resetCwdIfOutsideProject");
  require(bool(services.extractHints),"extractHints");require(bool(services.recordHint),"recordHint");
  require(bool(services.resizeImage),"resizeImage");require(bool(services.applySedEdit),"applySedEdit");
  require(bool(services.parseSedEdit),"parseSedEdit");
  require(bool(services.fileEditUserFacingName),"fileEditUserFacingName");
  require(bool(services.truncateSummary),"truncateSummary");
  require(bool(services.getGadgetResultPath),"getToolResultPath");
  require(bool(services.registerBackgroundMission),"registerBackgroundTask");
  if(services.showBackgroundHint){
    require(bool(services.registerForegroundMission),"registerForegroundTask");
    require(bool(services.unregisterForegroundMission),"unregisterForegroundTask");
    require(bool(services.clearGadgetJSX),"clearToolJSX");
  }
  if(services.missionOutputPoller){
    require(bool(services.registerForegroundMission),"registerForegroundTask");
    require(bool(services.unregisterForegroundMission),"unregisterForegroundTask");
    require(bool(services.backgroundExistingForegroundMission),"backgroundExistingForegroundTask");
    require(bool(services.markMissionNotified),"markTaskNotified");
  }
  require(bool(services.renderUse),"renderUse");require(bool(services.renderResult),"renderResult");
  require(bool(services.renderProgress),"renderProgress");require(bool(services.renderQueued),"renderQueued");
  require(bool(services.renderError),"renderError");
  auto shared=std::make_shared<Services>(std::move(services));
  gadgets::Gadget gadget;gadget.name="Bash";gadget.searchHint="execute shell commands";
  gadget.maxResultSizeChars=30000;gadget.strict=true;
  gadget.inputJSONSchema=shared->inputJSONSchema;gadget.outputSchema=shared->outputJSONSchema;
  gadget.parseInput=shared->parseInput;
  gadget.description=[](const Json& input,const Json&){
    std::promise<std::string> value;
    const auto description=stringField(input,"description");
    value.set_value(description.empty()?"Run shell command":description);
    return value.get_future();
  };
  gadget.poetry=shared->poetry;
  gadget.isReadOnly=[shared](const Json& input){return shared->readOnlyAllowed(input,
    shared->commandHasAnyCd(stringField(input,"command")));};
  gadget.isConcurrencySafe=gadget.isReadOnly;
  gadget.toAutoClassifierInput=[](const Json& input)->Json{return stringField(input,"command");};
  gadget.preparePermissionMatcher=shared->preparePermissionMatcher;
  gadget.checkPermissions=shared->checkPermissions;
  gadget.isSearchOrReadCommand=[shared](const Json& input){
    try{const auto parsed=shared->parseInput(input);
      return isSearchOrReadBashCommand(stringField(parsed,"command"),shared->splitWithOperators);
    }catch(...){return Json{{"isSearch",false},{"isRead",false},{"isList",false}};}
  };
  gadget.userFacingName=[shared](const Json& input){
    if(!input.is_object())return std::string("Bash");
    const auto command=stringField(input,"command");
    if(!command.empty()){
      const auto edit=shared->parseSedEdit(command);
      if(!edit.is_null())return shared->fileEditUserFacingName(
        Json{{"file_path",edit.at("filePath")},{"old_string","x"}});
    }
    return std::string(shared->sandboxIndicatorEnabled&&shared->shouldUseSandbox(input)?"SandboxedBash":"Bash");
  };
  gadget.getGadgetUseSummary=[shared](const Json& input)->Json{
    const auto command=stringField(input,"command");if(command.empty())return nullptr;
    const auto description=stringField(input,"description");
    return description.empty()?shared->truncateSummary(command,shared->gadgetSummaryMaxLength):description;
  };
  gadget.getActivityDescription=[shared](const Json& input)->Json{
    const auto command=stringField(input,"command");if(command.empty())return "Running command";
    const auto description=stringField(input,"description");
    return "Running "+(description.empty()?shared->truncateSummary(command,shared->gadgetSummaryMaxLength):description);
  };
  gadget.validateInput=[shared](const Json& input,const Json&){
    std::promise<gadgets::ValidationResult> value;
    gadgets::ValidationResult result;
    if(shared->monitorGadgetEnabled&&!shared->backgroundMissionsDisabled&&!boolField(input,"run_in_background")){
      auto pattern=detectBlockedSleepPattern(stringField(input,"command"),shared->splitCommand);
      if(pattern){result.result=false;result.errorCode=10;
        result.message="Blocked: "+*pattern+". Run blocking commands in the background with run_in_background: true — you'll get a completion notification when done. For streaming events (watching logs, polling APIs), use the Monitor tool. If you genuinely need a delay (rate limiting, deliberate pacing), keep it under 2 seconds.";}
    }
    value.set_value(result);return value.get_future();
  };
  gadget.renderGadgetUseMessage=shared->renderUse;gadget.renderGadgetResultMessage=shared->renderResult;
  gadget.renderGadgetUseProgressMessage=shared->renderProgress;
  gadget.renderGadgetUseQueuedMessage=shared->renderQueued;
  gadget.renderGadgetUseErrorMessage=shared->renderError;
  gadget.extractSearchText=[](const Json& output){const auto out=stringField(output,"stdout"),err=stringField(output,"stderr");
    return err.empty()?out:out+"\n"+err;};
  gadget.mapGadgetResultToGadgetResultBlockParam=[shared](const Json& output,const std::string& id){
    return mapGadgetResult(output,id,[shared](const std::string& missionId){return shared->shell->outputPath(missionId);},
      shared->assistantBlockingBudgetMs);};
  gadget.isResultTruncated=[](const Json& output){return isOutputTruncated(stringField(output,"stdout"))||
    isOutputTruncated(stringField(output,"stderr"));};
  gadget.call=[shared](const Json& input,const gadgets::GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json& parent,
    const std::function<void(const gadgets::GadgetProgress&)>& progress){
    return ::batbox::runtime::contextAsync(std::launch::async,[shared,input,context,parent,progress]{
      return callBash(input,context,parent,progress,*shared);
    });
  };
  return gadgets::buildGadget(std::move(gadget));
}
void bindIndigenousCommandSplitters(Services& services){
  services.splitCommand=[](const std::string& command){
    return shell_split::splitCommandDeprecated(command);};
  services.splitWithOperators=[](const std::string& command){
    return shell_split::splitCommandWithOperators(command);};
}
void bindIndigenousBashSchema(Services& services,double maxTimeoutMs){
  const bool disabled=services.backgroundMissionsDisabled;
  services.parseInput=[disabled](const Json& input){return parseBashInput(input,disabled);};
  services.inputJSONSchema=bashInputJSONSchema(maxTimeoutMs,disabled);
  services.outputJSONSchema=bashOutputJSONSchema();
}
void bindIndigenousHintParser(Services& services,HintProtocol protocol){
  if(protocol.tagName.empty()||!protocol.logDebug)
    throw std::invalid_argument("hint protocol tagName and logDebug required");
  services.extractHints=[protocol=std::move(protocol)](const std::string& output,
    const std::string& command){return extractBatboxHints(output,command,protocol);};
}
void bindIndigenousCwdPolicy(Services& services,CwdPolicyServices policy){
  if(!policy.getCwd||!policy.getOriginalCwd||!policy.shouldMaintainProjectWorkingDir||
     !policy.pathInAllowedWorkingPath||!policy.setCwd)
    throw std::invalid_argument("Bash cwd policy imports required");
  services.resetCwdIfOutsideProject=[policy=std::move(policy)](const Json& context){
    return resetCwdIfOutsideProject(context,policy);};
}
} // namespace batbox::bash_gadget
