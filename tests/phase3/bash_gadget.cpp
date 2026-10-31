#include <cstdlib>
#include "gadgets/bash/BashGadget.hpp"
#include "missions/shell/Roster.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <unistd.h>
using namespace batbox;
using namespace batbox::bash_gadget;
namespace {
template<class T>std::future<T> ready(T value){std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();}
void check(bool truth,const char* message){if(!truth)throw std::runtime_error(message);}
}
int main(){
  char temp[]="/tmp/batbox-bash-tool-test-XXXXXX";
  if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=std::filesystem::canonical(temp);
  try{
    bool rejectedIncomplete=false;
    try{(void)makeBashGadget(Services{});}catch(const std::invalid_argument&){rejectedIncomplete=true;}
    check(rejectedIncomplete,"incomplete Bash services were accepted");
    std::atomic<int> ids{0};
    std::mutex notificationsMutex;std::condition_variable notificationsCv;
    std::vector<std::string> notifications;
    shell_missions::Services missionServices;
    missionServices.nowMs=[](){return 1234LL;};
    missionServices.enqueueNotification=[&](const std::string& message,const std::optional<std::string>&){
      {std::lock_guard lock(notificationsMutex);notifications.push_back(message);}
      notificationsCv.notify_all();};
    missionServices.onMissionTerminated=[](const shell_missions::MissionRecord&){};
    auto roster=std::make_shared<shell_missions::Roster>(missionServices);
    shell::FlightConfig config;config.shellPath="/bin/bash";config.originalCwd=root.string();
    config.outputDir=(root/"jobs").string();config.maxInlineOutputBytes=100;
    config.generateMissionId=[&]{return "task"+std::to_string(++ids);};
    config.splitCommand=[](const std::string& command){
      return std::vector<std::string>{command.rfind("sleep ",0)==0?"sleep":command};};
    config.onBackgroundComplete=[weak=std::weak_ptr(roster)](const std::string& id,const shell::ExecResult& result){
      if(auto missions=weak.lock())missions->completeShell(id,result);};
    Services service;service.shell=std::make_shared<shell::ManagedShell>(config);
    roster->attachShell(service.shell);
    service.originalCwd=root.string();
    service.splitCommand=[](const std::string& command){return std::vector<std::string>{command};};
    service.splitWithOperators=service.splitCommand;
    service.parseInput=[](const Json& input){return input;};
    service.inputJSONSchema={{"type","object"}};service.outputJSONSchema={{"type","object"}};
    service.poetry=[](const gadgets::PoetryContext&){return ready(std::string("Run shell commands"));};
    std::atomic<int> permissionChecks{0};
    service.checkPermissions=[&](const Json& input,const Json&){++permissionChecks;
      return ready(Json{{"behavior","allow"},{"updatedInput",input}});};
    service.readOnlyAllowed=[](const Json&,bool hasCd){return !hasCd;};
    service.commandHasAnyCd=[](const std::string& command){return command.find("cd ")!=std::string::npos;};
    service.preparePermissionMatcher=[](const Json&){
      return ready(std::function<bool(const std::string&)>([](const std::string&){return true;}));};
    service.shouldUseSandbox=[](const Json&){return false;};
    service.sandboxCommand=[](const std::string& command,const Json&,const Json&){return command;};
    service.annotateSandboxFailures=[](const std::string&,const std::string& text){return text;};
    service.resetCwdIfOutsideProject=[](const Json&){return false;};
    service.extractHints=[](const std::string& text,const std::string&){
      return Json{{"stripped",text},{"hints",Json::array()}};};
    std::vector<Json> recordedHints;
    service.recordHint=[&](const Json& hint){recordedHints.push_back(hint);};
    service.resizeImage=[](const std::string& text,const shell::ExecResult&){return std::optional(text);};
    service.applySedEdit=[](const Json&,const gadgets::GadgetCallContext&,const Json&){
      throw std::runtime_error("sed edit test branch not configured");return gadgets::GadgetResult{};};
    service.parseSedEdit=[](const std::string&){return Json();};
    service.fileEditUserFacingName=[](const Json&){return std::string("Edit");};
    service.truncateSummary=[](const std::string& value,std::size_t max){
      return value.size()<=max?value:value.substr(0,max-3)+"...";};
    service.getGadgetResultPath=[&](const std::string& id){return (root/"results"/(id+".txt")).string();};
    service.registerBackgroundMission=[roster](const std::string& id,const std::string& command,
      const std::string& description,const Json& context){
      shell_missions::MissionRecord mission;mission.id=id;mission.command=command;mission.description=description;
      if(context.is_object()&&context.contains("toolUseId")&&context["toolUseId"].is_string())
        mission.gadgetUseId=context["toolUseId"].get<std::string>();
      roster->adoptBackground(std::move(mission));};
    service.renderUse=[](const Json&,const Json&){return Json("use");};
    service.renderResult=[](const Json&,const Json&,const Json&){return Json("result");};
    service.renderProgress=[](const Json&,const Json&){return Json("progress");};
    service.renderQueued=[](){return Json("queued");};
    service.renderError=[](const Json&,const Json&){return Json("error");};
    const auto gadget=makeBashGadget(service);
    check(gadget.name=="Bash"&&gadget.call&&gadget.checkPermissions&&gadget.renderGadgetUseMessage,"Tool methods missing");
    check(!gadget.isReadOnly(Json{{"command","cd x"}}),"readonly callback missing");
    check(gadget.checkPermissions(Json{{"command","echo hi"}},Json()).get()["behavior"]=="allow"&&permissionChecks==1,
      "permission callback missing");
    gadgets::GadgetCallContext context;context.data=Json::object();
    const auto result=gadget.call(Json{{"command","printf 'hello 🌍\\n'"}},context,{},Json(),{}).get();
    check(result.data["stdout"].get<std::string>()=="hello 🌍","real shell result mismatch");
    check(gadget.mapGadgetResultToGadgetResultBlockParam(result.data,"toolu_1")["content"]=="hello 🌍",
      "tool result mapping mismatch");
    const auto noMatch=gadget.call(Json{{"command","grep absent /dev/null"}},context,{},Json(),{}).get();
    check(noMatch.data["returnCodeInterpretation"]=="No matches found","semantic exit code mismatch");
    bool shellError=false;
    try{gadget.call(Json{{"command","exit 7"}},context,{},Json(),{}).get();}
    catch(const ShellError& error){shellError=error.code==7&&error.stderrText.empty();}
    check(shellError,"ShellError semantics mismatch");
    const auto large=gadget.call(Json{{"command","printf '%0200d' 0"}},context,{},Json(),{}).get();
    check(large.data.contains("persistedOutputPath")&&
      std::filesystem::exists(large.data["persistedOutputPath"].get<std::string>()),"large output not persisted");
    check(large.data["persistedOutputSize"]==200,
      "persisted output must report stat size before any truncation");
    const auto background=gadget.call(Json{{"command","sleep 0.1; echo done"},{"run_in_background",true}},
      context,{},Json(),{}).get();
    const auto missionId=background.data["backgroundTaskId"].get<std::string>();
    check(gadget.mapGadgetResultToGadgetResultBlockParam(background.data,"toolu_2")["content"]
      .get<std::string>().find(missionId)!=std::string::npos,"background model result missing ID");
    check(service.shell->wait(missionId,2000).stdoutText.find("done")!=std::string::npos,"background command failed");
    check(roster->wait(missionId,2000,{})->status=="completed","background task status not registered");
    {std::unique_lock lock(notificationsMutex);
      notificationsCv.wait_for(lock,std::chrono::seconds(2),[&]{return !notifications.empty();});
      check(!notifications.empty()&&notifications.back().find("<task-id>"+missionId+"</task-id>")!=std::string::npos,
        "background completion notification missing");}
    service.shell->cleanup(missionId);
    std::stop_source backgroundStop;
    gadgets::GadgetCallContext backgroundContext{Json::object(),backgroundStop.get_token(),
      []{return std::string("cancel");}};
    auto cancellableBackground=gadget.call(Json{{"command","sleep 20"},{"run_in_background",true}},
      backgroundContext,{},Json(),{}).get();
    auto cancellableId=cancellableBackground.data["backgroundTaskId"].get<std::string>();
    backgroundStop.request_stop();
    const auto backgroundKilled=service.shell->wait(cancellableId,2000);
    check(backgroundKilled.code==137&&backgroundKilled.interrupted,"background stop token did not kill process group");
    service.shell->cleanup(cancellableId);
    std::stop_source stop;gadgets::GadgetCallContext cancellable{Json::object(),stop.get_token(),
      []{return std::string("cancel");}};
    auto future=gadget.call(Json{{"command","sleep 20"}},cancellable,{},Json(),{});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));stop.request_stop();
    bool cancelled=false;
    try{(void)future.get();}
    catch(const ShellError& error){cancelled=error.interrupted&&error.code==137;}
    check(cancelled,"stop token did not cancel real process with ShellError");
    bindIndigenousCommandSplitters(service);
    bindIndigenousBashSchema(service,600000);
    bindIndigenousHintParser(service,{"batbox-hint",[](const std::string&){}});
    CwdPolicyServices cwdPolicy;
    cwdPolicy.getCwd=[shell=service.shell]{return shell->cwd();};
    cwdPolicy.getOriginalCwd=[path=root.string()]{return path;};
    cwdPolicy.shouldMaintainProjectWorkingDir=[](){return false;};
    cwdPolicy.pathInAllowedWorkingPath=[path=root.string()](const std::string& cwd,const Json&){
      return cwd==path||cwd.rfind(path+"/",0)==0;};
    cwdPolicy.setCwd=[shell=service.shell](const std::string& path){shell->setCwd(path);};
    bindIndigenousCwdPolicy(service,std::move(cwdPolicy));
    std::atomic<int> foregroundShows{0},foregroundClears{0};
    service.registerForegroundMission=[roster](const std::string& id,const std::string& command,
      const std::string& description,const Json&){
      shell_missions::MissionRecord mission;mission.id=id;mission.command=command;mission.description=description;
      roster->registerForeground(std::move(mission));};
    service.unregisterForegroundMission=[roster](const std::string& id){
      check(roster->unregisterForeground(id),"foreground task did not unregister");};
    service.showBackgroundHint=[&](const Json&){++foregroundShows;};
    service.clearGadgetJSX=[&](const Json&){++foregroundClears;};
    const auto indigenousSplitGadget=makeBashGadget(service);
    check(indigenousSplitGadget.parseInput(Json{{"command","sleep 1"},{"timeout","30"},
      {"run_in_background","false"}})["timeout"]==30.0,
      "native Bash semantic input parser mismatch");
    const auto heredoc=indigenousSplitGadget.call(Json{{"command","cat <<'EOF'\nhello heredoc\nEOF"}},
      context,{},Json(),{}).get();
    check(heredoc.data["stdout"]=="hello heredoc","native splitter heredoc execution mismatch");
    const auto hinted=indigenousSplitGadget.call(Json{{"command",
      "printf 'before\\n<batbox-hint v=1 type=plugin value=foo@bar />\\nafter\\n'"}},
      context,{},Json(),{}).get();
    check(hinted.data["stdout"].get<std::string>().find("batbox-hint")==std::string::npos&&
      recordedHints.size()==1&&recordedHints.back()["value"]=="foo@bar",
      "native hint parser did not strip output and record local hint");
    const auto movedCwd=indigenousSplitGadget.call(Json{{"command","cd /tmp; pwd"}},
      context,{},Json(),{}).get();
    check(movedCwd.data["stdout"]=="/tmp"&&
      movedCwd.data["stderr"]=="\nShell cwd was reset to "+root.string()&&
      service.shell->cwd()==root.string(),"native cwd policy failed to restore project directory");
    const auto foreground=indigenousSplitGadget.call(Json{{"command","sleep 2.2; echo foreground"}},
      context,{},Json(),{}).get();
    check(foreground.data["stdout"]=="foreground"&&foregroundShows>=1&&foregroundClears>=2&&
      roster->foregroundShellIds().empty(),"Bash foreground UI task lifecycle mismatch");
    bool progressFailed=false;
    try{indigenousSplitGadget.call(Json{{"command","sleep 20"}},context,{},Json(),
      [](const gadgets::GadgetProgress&){throw std::runtime_error("progress sink failed");}).get();}
    catch(const std::runtime_error& error){progressFailed=std::string(error.what())=="progress sink failed";}
    check(progressFailed&&roster->foregroundShellIds().empty()&&foregroundClears>=3,
      "throwing progress sink did not stop and clean up foreground command");
    service.missionOutputPoller=std::make_shared<shell::MissionOutputPoller>();
    service.backgroundExistingForegroundMission=[roster](const std::string& id){
      return roster->backgroundExistingForeground(id);};
    service.markMissionNotified=[roster](const std::string& id){(void)roster->markNotified(id);};
    const auto managedRunnerGadget=makeBashGadget(service);
    const auto managedQuick=managedRunnerGadget.call(Json{{"command","echo managed quick"}},
      context,{},Json(),{}).get();
    check(managedQuick.data["stdout"]=="managed quick","managed runner quick path failed");
    const auto managedBackground=managedRunnerGadget.call(Json{{"command","sleep 0.1; echo managed bg"},
      {"run_in_background",true}},context,{},Json(),{}).get();
    const auto managedBackgroundId=managedBackground.data["backgroundTaskId"].get<std::string>();
    check(roster->wait(managedBackgroundId,2000,{})->status=="completed",
      "managed runner background registration failed");
    service.shell->cleanup(managedBackgroundId);
    const auto managedSlow=managedRunnerGadget.call(Json{{"command","sleep 3.2; echo managed slow"}},
      context,{},Json(),{}).get();
    check(managedSlow.data["stdout"]=="managed slow"&&roster->foregroundShellIds().empty(),
      "managed runner progress/foreground cleanup failed");
    std::filesystem::remove_all(root);
    std::cout<<"PASS Bash Tool process/result/background/cancellation/heredoc/foreground UI cleanup\n";
    return 0;
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;
  }
}
