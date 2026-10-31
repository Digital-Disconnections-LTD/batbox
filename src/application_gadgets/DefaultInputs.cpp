#include "application_gadgets/DefaultInputs.hpp"
#include "application_shell/RuleMatch.hpp"
#include "formatting/Format.hpp"
#include "mcp/Collapse.hpp"
#include "mcp/OutputStorage.hpp"
#include "mcp/ProcessResult.hpp"
#include "services/tokenEstimation.hpp"
#include "settings/schema/IndigenousBindings.hpp"
#include "gadgets/mission_output/MissionOutput.hpp"
#include "gadgets/mission_stop/MissionStop.hpp"
#include "gadgets/mission_management/Gadgets.hpp"
#include "runtime/JsonValue.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <set>
#include <stdexcept>

namespace batbox::application_gadgets {
namespace {
std::string missionEnvironment(const char* name){
#ifdef _WIN32
  return runtime::environmentUtf8(name);
#else
  const char* value=std::getenv(name);return value?value:"";
#endif
}
std::int64_t nowMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(
  std::chrono::system_clock::now().time_since_epoch()).count();}
std::string field(const Json& value,const char* name){
  const auto& item=value::property(value,name);
  return item.is_string()?item.get<std::string>():"";
}
std::string visible(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_null())return "";
  if(value.is_object()){
    for(const char* key:{"text","content","stdout","stderr","message","output"}){
      const auto& part=value::property(value,key);
      if(part.is_string()&&!part.get<std::string>().empty())return part.get<std::string>();
    }
  }
  return value.dump(2);
}
std::string target(const Json& input){
  for(const char* key:{"file_path","path","pattern","query","url","command"}){
    const auto text=field(input,key);if(!text.empty())return text;
  }
  return visible(input);
}
bool isSettingsFile(const std::string& file){
  auto path=std::filesystem::path(file).lexically_normal().generic_string();
  std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){
    return static_cast<char>(std::tolower(c));});
  return path.ends_with("/.claude/settings.json")||
    path.ends_with("/.claude/settings.local.json");
}
std::string settingsIssues(const Json& issues){
  std::string result;
  if(!issues.is_array())return "Settings do not match the native schema";
  for(const auto& issue:issues){
    if(!result.empty())result+='\n';
    result+="- ";
    const auto& path=value::property(issue,"path");
    if(path.is_array())for(const auto& part:path){
      if(result.back()!=' ')result+='.';
      result+=part.is_string()?part.get<std::string>():value::stringify(part);
    }
    if(result.back()!=' ')result+=": ";
    const auto& message=value::property(issue,"message");
    result+=message.is_string()?message.get<std::string>():"Invalid setting";
  }
  return result.empty()?"Settings do not match the native schema":result;
}
std::vector<std::string> lines(const std::string& source){
  std::vector<std::string> result;std::size_t start=0;
  while(start<source.size()){
    const auto end=source.find('\n',start);
    result.push_back(source.substr(start,end==std::string::npos?end:end-start));
    if(end==std::string::npos)break;
    start=end+1;
  }
  return result;
}
Json patch(const std::string&,const std::string& before,const std::string& after){
  const auto old=lines(before),fresh=lines(after);
  std::size_t prefix=0;
  while(prefix<std::min(old.size(),fresh.size())&&old[prefix]==fresh[prefix])++prefix;
  if(prefix==old.size()&&prefix==fresh.size())return Json::array();
  std::size_t suffix=0;
  while(suffix<old.size()-prefix&&suffix<fresh.size()-prefix&&
      old[old.size()-1-suffix]==fresh[fresh.size()-1-suffix])++suffix;
  const auto contextBefore=std::min<std::size_t>(3,prefix);
  const auto contextAfter=std::min<std::size_t>(3,suffix);
  Json body=Json::array();
  for(std::size_t i=prefix-contextBefore;i<prefix;++i)body.push_back(" "+old[i]);
  for(std::size_t i=prefix;i<old.size()-suffix;++i)body.push_back("-"+old[i]);
  for(std::size_t i=prefix;i<fresh.size()-suffix;++i)body.push_back("+"+fresh[i]);
  for(std::size_t i=old.size()-contextAfter;i<old.size();++i)body.push_back(" "+old[i]);
  return Json::array({{{"oldStart",prefix-contextBefore+1},
    {"oldLines",old.size()-prefix-suffix+contextBefore+contextAfter},
    {"newStart",prefix-contextBefore+1},
    {"newLines",fresh.size()-prefix-suffix+contextBefore+contextAfter},
    {"lines",body}}});
}
std::function<void(const std::function<void()>&)> ownerCall(event_loop::Loop& owner){
  auto dispatch=owner.dispatcher();
  return [dispatch](const std::function<void()>& work){
    if(dispatch.isOwnerThread()){work();return;}
    auto done=std::make_shared<std::promise<void>>();auto result=done->get_future();
    dispatch.postMission([work,done]{try{work();done->set_value();}
      catch(...){done->set_exception(std::current_exception());}});
    result.get();
  };
}
Json currentPermission(const std::shared_ptr<app_state::AppStateStore>& store){
  const auto state=store->getState();
  return state&&state->permissionContext?state->permissionContext->snapshot():Json::object();
}
std::future<Json> readyDecision(Json value){
  std::promise<Json> promise;auto result=promise.get_future();
  promise.set_value(std::move(value));return result;
}
bool underDirectory(const std::filesystem::path& source,
    const std::filesystem::path& directory){
  const auto relative=source.lexically_relative(directory);
  return !relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..";
}
bool inWorkingDirectories(const std::string& requested,const std::string& cwd,
    const Json& context){
  const auto absolute=std::filesystem::path(requested).is_absolute()
    ?std::filesystem::path(requested):(std::filesystem::path(cwd)/requested);
  const auto lexical=absolute.lexically_normal();
  std::vector<std::filesystem::path> roots={cwd};
  const auto& nested=value::property(context,"toolPermissionContext");
  const auto& permission=nested.is_object()?nested:context;
  const auto& additional=value::property(permission,"additionalWorkingDirectories");
  if(additional.is_object())for(auto it=additional.begin();it!=additional.end();++it)
    roots.emplace_back(it.key());
  else if(additional.is_array())for(const auto& item:additional)
    if(item.is_string())roots.emplace_back(item.get<std::string>());
  std::error_code error;
  const auto resolved=std::filesystem::weakly_canonical(lexical,error);
  if(error)return false;
  for(const auto& root:roots){
    const auto base=std::filesystem::absolute(root).lexically_normal();
    if(!underDirectory(lexical,base))continue;
    error.clear();const auto canonical=std::filesystem::weakly_canonical(base,error);
    if(!error&&underDirectory(resolved,canonical))return true;
  }
  return false;
}
bool fileRuleMatches(const application_permissions::PermissionProvider& permission,
    const std::string& gadget,const std::string& requested,const std::string& cwd,
    const Json& context,const std::string& behavior){
  const auto absolute=(std::filesystem::path(requested).is_absolute()
    ?std::filesystem::path(requested):(std::filesystem::path(cwd)/requested))
    .lexically_normal();
  std::set<std::string> candidates={requested,absolute.string()};
  const auto relative=absolute.lexically_relative(cwd);
  if(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!=".."){
    candidates.insert(relative.generic_string());
    candidates.insert("./"+relative.generic_string());
  }
  for(const auto& path:candidates)if(permission.matchingRule(gadget,path,context,behavior))
    return true;
  return false;
}
std::vector<std::string> readDenyPatterns(const Json& context,
    const std::string& configRoot,const std::string& home){
  const auto& nested=value::property(context,"toolPermissionContext");
  const auto& rules=value::property(nested.is_object()?nested:context,"alwaysDenyRules");
  std::set<std::string> patterns;
  auto scan=[&](const Json& entries){
    if(!entries.is_array())return;
    for(const auto& raw:entries){
      if(!raw.is_string())continue;
      const auto rule=raw.get<std::string>();
      if(rule=="Read"){patterns.insert("**/*");continue;}
      if(!rule.starts_with("Read(")||!rule.ends_with(')'))continue;
      auto value=rule.substr(5,rule.size()-6);
      if(value.starts_with("//"))value=value.substr(1);
      else if(value.starts_with("~/"))value=(std::filesystem::path(home)/value.substr(2)).string();
      else if(value.starts_with('/'))value=(std::filesystem::path(configRoot)/value.substr(1)).string();
      else if(value.starts_with("./"))value=value.substr(2);
      if(value.ends_with("/**"))value.resize(value.size()-3);
      patterns.insert(value);
    }
  };
  scan(rules);
  if(rules.is_object())for(auto it=rules.begin();it!=rules.end();++it)scan(it.value());
  return {patterns.begin(),patterns.end()};
}
std::vector<std::string> normalizeReadPatterns(const std::vector<std::string>& patterns,
    const std::string& root){
  std::set<std::string> output;
  const auto base=std::filesystem::path(root).lexically_normal();
  for(const auto& pattern:patterns){
    const auto path=std::filesystem::path(pattern).lexically_normal();
    if(!path.is_absolute()){output.insert(pattern);continue;}
    const auto relative=path.lexically_relative(base);
    if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")continue;
    output.insert("/"+relative.generic_string());
  }
  return {output.begin(),output.end()};
}
gadgets::files::FileGadgetBindings fileBinding(const std::string& name,
    const std::shared_ptr<application_permissions::PermissionProvider>& permission,
    const std::string& cwd){
  gadgets::files::FileGadgetBindings result;
  result.checkPermissions=[permission,name,cwd](const Json& input,const Json& context){
    const bool read=name=="Read"||name=="Glob"||name=="Grep";
    const auto requested=read?(name=="Read"?field(input,"file_path"):field(input,"path")):
      field(input,"file_path");
    const auto path=requested.empty()?cwd:requested;
    const auto ruleGadget=read?"Read":"Edit";
    const auto denied=fileRuleMatches(*permission,ruleGadget,path,cwd,context,"deny")||
      fileRuleMatches(*permission,name,path,cwd,context,"deny");
    if(denied)return readyDecision(Json{{"behavior","deny"},
      {"message","Permission denied by file rule"}});
    const auto& nested=value::property(context,"toolPermissionContext");
    const auto& settings=nested.is_object()?nested:context;
    const auto mode=field(settings,"mode");
    if(!read&&mode=="plan")return permission->check(name,input,context);
    const bool ask=fileRuleMatches(*permission,ruleGadget,path,cwd,context,"ask")||
      fileRuleMatches(*permission,name,path,cwd,context,"ask");
    const bool inside=inWorkingDirectories(path,cwd,context);
    const bool explicitAllow=fileRuleMatches(*permission,ruleGadget,path,cwd,context,"allow")||
      fileRuleMatches(*permission,name,path,cwd,context,"allow");
    if(!ask&&(explicitAllow||(read&&inside)||(!read&&mode=="acceptEdits"&&inside)))
      return readyDecision(Json{{"behavior","allow"},{"updatedInput",input},
        {"decisionReason",explicitAllow?Json{{"type","rule"}}:
          Json{{"type","mode"},{"mode",read?"default":"acceptEdits"}}}});
    return permission->check(name,input,context);};
  result.matchWildcardPattern=[](const std::string& pattern,const std::string& value){
    return application_shell::matchWildcardPattern(pattern,value);};
  result.userFacingName=[name](const Json&){return name;};
  result.getGadgetUseSummary=[](const Json& input)->Json{return target(input);};
  result.renderUse=[name](const Json& input,const Json&)->Json{return name+": "+target(input);};
  result.renderResult=[](const Json&,const Json& output,const Json&)->Json{return visible(output);};
  result.renderError=[](const Json& error,const Json&)->Json{return visible(error);};
  result.renderRejected=[](const Json& reason,const Json&)->Json{return visible(reason);};
  result.renderTag=[](const Json&)->Json{return Json();};
  result.isResultTruncated=[](const Json& output){
    return value::property(output,"truncated")==true;};
  return result;
}
gadgets::web::UI webUi(const std::string& name){
  gadgets::web::UI result;
  result.getGadgetUseSummary=[](const Json& input)->Json{return target(input);};
  result.renderUse=[name](const Json& input,const Json&)->Json{return name+": "+target(input);};
  result.renderProgress=[](const Json& progress,const Json&)->Json{return visible(progress);};
  result.renderResult=[](const Json&,const Json& output,const Json&)->Json{return visible(output);};
  return result;
}
}

Inputs makeDefaultInputs(DefaultInputs d){
  if(!d.store||!d.messages||!d.fileStash||!d.permission||!d.connectors||
      !d.model||!d.shell.shell||!d.shell.poller||!d.shell.missions)
    throw std::invalid_argument("Native application tool defaults require live owners");
  auto onOwner=ownerCall(d.owner);
  Inputs out;
  const auto cwd=d.config.cwd.string();
  out.cwd=[shell=d.shell.shell]{return shell->cwd();};
  out.homeDirectory=[](){const char* value=std::getenv("HOME");return value?std::string(value):std::string();};
  out.messages=d.messages;
  auto& files=out.files;
  files.stash=d.fileStash;
  files.skillRoster=d.skillRoster?d.skillRoster:std::make_shared<skills::SkillRoster>();
  files.skillServices=std::move(d.skillServices);
  if(files.skillServices.homeDir.empty())files.skillServices.homeDir=out.homeDirectory();
  if(files.skillServices.configHomeDir.empty())
    files.skillServices.configHomeDir=application::configurationDirectory(d.config).string();
  if(!files.skillServices.isPathGitignored)
    files.skillServices.isPathGitignored=[](const std::string&,const std::string&){return false;};
  if(!files.skillServices.ignorePatternsMatch)
    files.skillServices.ignorePatternsMatch=[](const std::vector<std::string>& patterns,
        const std::string& path){
      for(const auto& pattern:patterns)
        if(application_shell::matchWildcardPattern(pattern,path))return true;
      return false;};
  files.runOnOwnerAndWait=onOwner;
  files.makePresentation=[permission=d.permission,cwd](const std::string& name){
    return fileBinding(name,permission,cwd);};
  files.presentation=fileBinding("Read",d.permission,cwd);
  files.readDenied=[permission=d.permission,store=d.store,onOwner,cwd](const std::string& path){
    bool denied=false;onOwner([&]{denied=fileRuleMatches(*permission,"Read",path,cwd,
      currentPermission(store),"deny");});
    return denied;};
  files.editDenied=[permission=d.permission,store=d.store,onOwner,cwd](const std::string& path){
    bool denied=false;onOwner([&]{const auto context=currentPermission(store);
      denied=field(context,"mode")=="plan"||
        fileRuleMatches(*permission,"Edit",path,cwd,context,"deny")||
        fileRuleMatches(*permission,"Write",path,cwd,context,"deny");});return denied;};
  files.readIgnorePatterns=[configRoot=application::configurationDirectory(d.config).string(),
      home=out.homeDirectory](const Json& context){
    return readDenyPatterns(context,configRoot,home());};
  files.normalizeIgnorePatterns=normalizeReadPatterns;
  files.pluginGlobExclusions=[](const std::string&){return std::vector<std::string>{};};
  files.patch=patch;
  files.countTokens=[](const std::string&)->std::optional<double>{return std::nullopt;};
  files.notebookFormatOutput=[](const std::string& value){return value;};
  files.isAutoMemoryFile=[](const std::string&){return false;};
  const auto validateSettings=settings::schema::bindIndigenousSettingsSchema(
    settings::schema::MainConfig{},settings::PermissionRuleServices{});
  files.validateSettingsEdit=[validateSettings](const std::string& path,
      const std::string& before,const std::function<std::string()>& updated)
      ->std::optional<gadgets::ValidationResult>{
    if(!isSettingsFile(path))return {};
    try{
      if(!validateSettings(Json::parse(before)).success)return {};
    }catch(...){return {};}
    std::string error;
    try{
      const auto after=validateSettings(Json::parse(updated()));
      if(after.success)return {};
      error=settingsIssues(after.issues);
    }catch(const std::exception& cause){error=std::string("Invalid JSON: ")+cause.what();}
    gadgets::ValidationResult failure;
    failure.result=false;failure.errorCode=10;
    failure.message="Batbox settings.json validation failed after edit:\n"+
      error+"\nIMPORTANT: Do not update the env unless explicitly instructed to do so.";
    return failure;
  };
  files.diagnosticBeforeEdit=[](const std::string&){};
  files.notifyEditor=[](const std::string&,const std::optional<std::string>&,
    const std::string&){};
  files.historyEnabled=[history=d.fileHistory]{return history&&history->enabled();};
  if(d.fileHistory)files.trackHistory=[history=d.fileHistory](
      const std::string& path,const std::string& messageId){
    history->trackEdit(path,messageId);
  };
  files.lspAvailable=[lsp=d.lsp]{return lsp&&lsp->available();};
  if(d.lsp){
    files.clearDiagnostics=[lsp=d.lsp](const std::string& uri){lsp->clearDeliveredDiagnostics(uri);};
    files.lspChange=[lsp=d.lsp](const std::string& path,const std::string& content){lsp->changeFile(path,content);};
    files.lspSave=[lsp=d.lsp](const std::string& path){lsp->saveFile(path);};
    const auto readPermission=fileBinding("Read",d.permission,d.config.cwd.string()).checkPermissions;
    d.additionalGadgets.push_back(d.lsp->makeGadget([readPermission](const Json& input,const Json& context){
      Json readInput=input;readInput["file_path"]=value::property(input,"filePath");
      auto decision=readPermission(readInput,context);
      return std::async(std::launch::deferred,[input,decision=std::move(decision)]()mutable{
        auto value=decision.get();
        if(value.value("behavior",std::string{})=="allow")value["updatedInput"]=input;
        return value;
      });
    }));
  }
  files.remoteDiffEnabled=[](){return false;};
  files.checkTeamMemorySecrets=[](const std::string&,
    const std::string&)->std::optional<std::string>{return std::nullopt;};
  files.localeCompare=[](const std::string& a,const std::string& b){return a.compare(b);};
  files.nowMs=[](){return static_cast<double>(nowMs());};
  files.media.getGadgetResultsDir=[storage=d.storage]{return gadget_results::getGadgetResultsDir(storage);};
  files.media.randomUUID=d.messages->randomUUID;
  files.media.isPDFSupported=[](){return false;};
  files.ripgrep.command="rg";

  auto& bash=out.bash;
  bash.owner=&d.owner;
  bash.hintTagName="batbox-command-hint";
  application_shell::bindBashMissionServices(bash.services,d.shell,d.storage,d.showBackgroundHint);
  application_shell::bindConservativeBashClassification(bash.services);
  d.permission->bindBash(bash.services);
  bash.services.poetry=[](const gadgets::PoetryContext&){
    std::promise<std::string> ready;ready.set_value("Execute a shell command in the current project.");
    return ready.get_future();};
  bash.services.shouldUseSandbox=[](const Json&){return false;};
  bash.services.sandboxCommand=[](const std::string& command,const Json&,const Json&){return command;};
  bash.services.getSandboxTmpDir=[state=d.config.stateDirectory]{return (state/"tmp").string();};
  bash.services.logDebug=d.log?d.log:[](const std::string&){};
  bash.services.recordHint=[log=bash.services.logDebug](const Json& hint){
    log("Bash command hint: "+hint.dump());};
  bash.services.annotateSandboxFailures=[](const std::string& text,const std::string&){return text;};
  bash.services.resizeImage=[](const std::string&,const shell::ExecResult&)->std::optional<std::string>{return std::nullopt;};
  bash.services.parseSedEdit=[](const std::string&)->Json{return Json();};
  bash.services.applySedEdit=[](const Json&,const gadgets::GadgetCallContext&,const Json&)->gadgets::GadgetResult{
    throw std::runtime_error("Native simulated sed edit is unavailable");};
  bash.services.fileEditUserFacingName=[](const Json&){return std::string("Edit");};
  bash.services.truncateSummary=[](const std::string& text,std::size_t limit){
    return text.size()>limit?text.substr(0,limit)+"…":text;};
  bash.services.renderUse=[](const Json& input,const Json&)->Json{return "$ "+target(input);};
  bash.services.renderResult=[](const Json&,const Json& output,const Json&)->Json{return visible(output);};
  bash.services.renderProgress=[](const Json& progress,const Json&)->Json{return visible(progress);};
  bash.services.renderQueued=[]()->Json{return "Queued";};
  bash.services.renderError=[](const Json& error,const Json&)->Json{return visible(error);};
  bash.services.clearGadgetJSX=[](const Json&){};
  bash.cwdPolicy.shouldMaintainProjectWorkingDir=[](){return false;};
  bash.cwdPolicy.pathInAllowedWorkingPath=[cwd](const std::string& path,const Json&){
    const auto root=std::filesystem::weakly_canonical(cwd);
    const auto candidate=std::filesystem::weakly_canonical(path);
    const auto relative=candidate.lexically_relative(root);
    return !relative.empty()&&!relative.is_absolute()&&
      *relative.begin()!=std::filesystem::path("..");};
  application_shell::AsyncHostInputs async{d.owner,d.shell,d.storage,{},{},{},{}};
  async.getGadgetPermissionContext=[store=d.store]{return currentPermission(store);};
  async.applySedEdit=[&owner=d.owner](const Json&,const gadgets::GadgetCallContext&,
      const Json&){event_loop::Deferred<gadgets::GadgetResult> result(owner);
    result.reject(std::make_exception_ptr(std::runtime_error(
      "Native simulated sed edit is unavailable")));return result;};
  async.onUnobservedError=d.onError?d.onError:[](std::exception_ptr){};
  bash.asyncMissionHost=application_shell::makeAsyncBashMissionHost(std::move(async));

  auto& web=out.web;
  web.searxng=d.connectors->searxng;web.scrapling=d.connectors->scrapling;
  web.searxngEndpoint=d.connectors->searxngEndpoint;
  web.scraplingCommand=application_connectors::scraplingServerArgv(d.config);
  web.search.ui=webUi("Web Search");
  web.fetch.ui=webUi("Web Fetch");
  web.fetch.skipPreflight=[](){return true;};
  web.fetch.permissionRule=[permission=d.permission](const Json& context,
      const std::string& behavior,const std::string& content){
    return permission->matchingRule("WebFetch",content,context,behavior);};
  web.makeQueryServices=[model=d.model]{return model->makeQueryServices();};
  web.getSmallFastModel=[model=d.model]{return model->config().smallModel;};

  out.bat=std::move(d.bat);
  out.makeBatGadget=std::move(d.makeBatGadget);
  out.additionalGadgets=std::move(d.additionalGadgets);
  mission_output::Services missionOutput;
  missionOutput.roster=d.shell.missions;
  missionOutput.nowMs=nowMs;
  missionOutput.getMissionOutputPath=[shell=d.shell.shell,missions=d.shell.missions](
      const std::string& id){
    if(const auto mission=missions->get(id);mission&&mission->outputFilePath)
      return *mission->outputFilePath;
    return shell->outputPath(id);};
  missionOutput.renderResult=[](const Json&,const Json& data,const Json&)->Json{
    const auto& mission=value::property(data,"task");
    if(!mission.is_object())return Json(visible(data));
    const auto& output=value::property(mission,"output");
    std::string text="Task "+field(mission,"task_id");
    const auto status=field(mission,"status");
    if(!status.empty())text+=" ("+status+")";
    const auto retrieval=field(data,"retrieval_status");
    if(retrieval=="timeout")text+=" — wait timed out";
    else if(retrieval=="not_ready")text+=" — still running";
    if(output.is_string()&&!output.get<std::string>().empty())
      text+='\n'+output.get<std::string>();
    const auto error=field(mission,"error");if(!error.empty())text+='\n'+error;
    return text.empty()?Json(visible(data)):Json(std::move(text));};
  missionOutput.renderProgress=[](const Json& data,const Json&)->Json{
    return visible(data);};
  missionOutput.renderError=[](const Json& data,const Json&)->Json{
    return visible(data);};
  missionOutput.renderRejected=missionOutput.renderError;
  missionOutput.renderTag=[](const Json&)->Json{return "Task Output";};
  out.additionalGadgets.push_back(mission_output::makeMissionOutputGadget(
    std::move(missionOutput)));
  mission_stop::Services missionStop;
  missionStop.roster=d.shell.missions;
  missionStop.renderUse=[](const Json& input,const Json&)->Json{
    const auto id=field(input,"task_id");
    return "Stop task "+(id.empty()?field(input,"shell_id"):id);};
  missionStop.renderResult=[](const Json&,const Json& data,const Json&)->Json{
    return visible(data);};
  out.additionalGadgets.push_back(mission_stop::makeMissionStopGadget(
    std::move(missionStop)));
  mission_management::Services missionList;
  missionList.store=std::make_shared<mission_management::Store>(
    application::configurationDirectory(d.config));
  missionList.missionListId=[flight=d.missionListId]{
    const auto override=missionEnvironment("BATBOX_TASK_LIST_ID");
    if(!override.empty())return override;
    if(!flight)throw std::logic_error("Current session task list ID is unavailable");
    return flight();};
  missionList.lifecycleHooks=std::move(d.missionLifecycleHooks);
  missionList.expandMissions=std::move(d.expandMissionList);
  const std::string flag=missionEnvironment("BATBOX_ENABLE_TASKS");
  missionList.enabled=!d.config.print||flag=="1"||flag=="true"||flag=="yes";
  for(auto& missionGadget:mission_management::makeMissionGadgets(std::move(missionList)))
    out.additionalGadgets.push_back(std::move(missionGadget));
  out.mcpConnections=d.connectors->mcpConnections;
  auto& mcpServices=out.mcpServices;
  mcpServices.recursivelySanitizeUnicode=mcp::recursivelySanitizeUnicode;
  mcpServices.classifyForCollapse=mcp::classifyMcpGadgetForCollapse;
  mcpServices.nowMs=nowMs;
  mcpServices.localizedError=[](const std::string& value){return value;};
  mcpServices.renderGadgetUseMessage=[](const Json& input,const Json&)->Json{return visible(input);};
  mcpServices.renderGadgetResultMessage=[](const Json&,const Json& output,const Json&)->Json{return visible(output);};
  mcpServices.renderGadgetUseProgressMessage=[](const Json& progress,const Json&)->Json{return visible(progress);};
  mcpServices.isOutputLineTruncated=[](const std::string& text){return text.find("[OUTPUT TRUNCATED")!=std::string::npos;};
  mcpServices.logError=d.log?d.log:[](const std::string&){};
  mcp::StorageConfig mcpStorage;
  mcpStorage.getGadgetResultsDir=[storage=d.storage]{return gadget_results::getGadgetResultsDir(storage);};
  mcpStorage.ensureGadgetResultsDir=[storage=d.storage]{
    std::filesystem::create_directories(gadget_results::getGadgetResultsDir(storage));};
  mcpStorage.nowMs=nowMs;
  mcpStorage.randomSuffix=d.messages->randomUUID;
  mcp::ProcessResultServices processor;
  mcp::bindIndigenousResultServices(processor.result,mcpStorage);
  processor.validation.configuredMaxOutputTokens=static_cast<double>(d.config.maxOutputTokens);
  processor.validation.countMessagesTokensWithAPI=[](const Json& messages,
      const Json&)->std::optional<double>{return roughTokenCountEstimation(messages.dump());};
  processor.validation.logError=mcpServices.logError;
  processor.nowMs=nowMs;
  processor.persistGadgetResult=[storage=d.storage](const std::string& content,
      const std::string& id)->Json{
    try{
      std::filesystem::create_directories(gadget_results::getGadgetResultsDir(storage));
      const auto path=gadget_results::getGadgetResultPath(storage,id,false);
      std::ofstream file(path,std::ios::binary|std::ios::trunc);
      if(!file)throw std::runtime_error("Could not open MCP output file");
      file.write(content.data(),static_cast<std::streamsize>(content.size()));
      if(!file)throw std::runtime_error("Could not write MCP output file");
      return {{"filepath",path},{"originalSize",content.size()}};
    }catch(const std::exception& error){return {{"error",error.what()}};}};
  mcp::bindIndigenousProcessResult(mcpServices,processor);
  return out;
}
} // namespace batbox::application_gadgets
