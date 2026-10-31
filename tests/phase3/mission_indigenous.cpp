#include <cstdlib>
#include "missions/shell/Roster.hpp"
#include "gadgets/mission_output/MissionOutput.hpp"
#include "gadgets/mission_stop/MissionStop.hpp"
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
namespace {
void check(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
shell_missions::MissionRecord mission(const std::string& id,const std::string& command){
  shell_missions::MissionRecord record;record.id=id;record.command=command;record.description=command;
  return record;
}
}
int main(){
  char temp[]="/tmp/batbox-task-test-XXXXXX";if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=temp;
  try{
    std::atomic<int> ids{0},terminated{0};
    std::mutex notificationMutex;std::condition_variable notificationCv;
    std::vector<std::string> notifications;
    shell_missions::Services services;
    services.nowMs=[](){return 1000LL;};
    services.enqueueNotification=[&](const std::string& message,const std::optional<std::string>&){
      {std::lock_guard lock(notificationMutex);notifications.push_back(message);}
      notificationCv.notify_all();};
    services.onMissionTerminated=[&](const shell_missions::MissionRecord&){++terminated;};
    auto roster=std::make_shared<shell_missions::Roster>(services);
    shell::FlightConfig config;config.shellPath="/bin/bash";config.originalCwd=root.string();
    config.outputDir=(root/"jobs").string();config.maxInlineOutputBytes=1000;
    config.generateMissionId=[&]{return "task"+std::to_string(++ids);};
    config.splitCommand=[](const std::string& command){return std::vector<std::string>{command};};
    config.onBackgroundComplete=[weak=std::weak_ptr(roster)](const std::string& id,
      const shell::ExecResult& result){if(auto current=weak.lock())current->completeShell(id,result);};
    auto shell=std::make_shared<shell::ManagedShell>(config);roster->attachShell(shell);
    mission_output::Services outputServices;outputServices.roster=roster;
    outputServices.nowMs=[](){return 12345LL;};
    outputServices.getMissionOutputPath=[shell](const std::string& id){return shell->outputPath(id);};
    outputServices.renderResult=[](const Json&,const Json&,const Json&){return Json("result");};
    outputServices.renderProgress=[](const Json&,const Json&){return Json("progress");};
    outputServices.renderError=[](const Json&,const Json&){return Json("error");};
    outputServices.renderRejected=[](const Json&,const Json&){return Json("rejected");};
    outputServices.renderTag=[](const Json&){return Json("tag");};
    auto outputGadget=mission_output::makeMissionOutputGadget(outputServices);
    mission_stop::Services stopServices;stopServices.roster=roster;
    stopServices.renderUse=[](const Json&,const Json&){return Json("use");};
    stopServices.renderResult=[](const Json&,const Json&,const Json&){return Json("result");};
    auto stopGadget=mission_stop::makeMissionStopGadget(stopServices);
    check(outputGadget.isReadOnly(Json())&&outputGadget.isConcurrencySafe(Json()),"TaskOutput readonly flags wrong");
    check(stopGadget.isConcurrencySafe(Json())&&stopGadget.shouldDefer.value_or(false),"TaskStop flags wrong");
    check(mission_output::parseInput(Json{{"task_id","x"},{"block","false"}})["block"]==false,
      "semantic block parsing failed");
    check(!stopGadget.validateInput(Json{{"task_id","missing"}},Json()).get().result,
      "TaskStop validated missing task");
    auto result=shell->run(shell::RunOptions{.command="sleep 0.2; echo done",.runInBackground=true});
    check(bool(result.backgroundMissionId),"background shell ID missing");
    const auto id=*result.backgroundMissionId;roster->registerShell(mission(id,"sleep 0.2; echo done"));
    gadgets::GadgetCallContext context{Json::object(),{},{}};
    const auto immediate=outputGadget.call(Json{{"task_id",id},{"block",false}},context,{},Json(),{}).get();
    check(immediate.data["retrieval_status"]=="not_ready","nonblocking task read wrong");
    std::atomic<int> progress{0};
    const auto waited=outputGadget.call(Json{{"task_id",id},{"block",true},{"timeout",2000}},
      context,{},Json(),[&](const gadgets::GadgetProgress& item){
        if(item.data["type"]=="waiting_for_task")++progress;}).get();
    check(waited.data["retrieval_status"]=="success"&&progress==1,
      "blocking task read failed");
    check(waited.data["task"]["output"].get<std::string>().find("done")!=std::string::npos,
      "TaskOutput did not read real process output");
    check(outputGadget.mapGadgetResultToGadgetResultBlockParam(waited.data,"toolu_1")["content"]
      .get<std::string>().find("<retrieval_status>success</retrieval_status>")!=std::string::npos,
      "TaskOutput model block missing");
    {std::unique_lock lock(notificationMutex);
      notificationCv.wait_for(lock,std::chrono::seconds(2),[&]{return !notifications.empty();});
      check(!notifications.empty()&&notifications.back().find("<task-id>"+id+"</task-id>")!=std::string::npos,
        "completion notice missing");}
    shell->cleanup(id);
    auto longRun=shell->run(shell::RunOptions{.command="sleep 20",.runInBackground=true});
    const auto longId=*longRun.backgroundMissionId;roster->registerShell(mission(longId,"sleep 20"));
    check(stopGadget.validateInput(Json{{"shell_id",longId}},Json()).get().result,
      "TaskStop deprecated shell_id validation failed");
    const auto stopped=stopGadget.call(Json{{"shell_id",longId}},context,{},Json(),{}).get();
    check(stopped.data["task_id"]==longId&&stopped.data["task_type"]=="local_bash",
      "TaskStop result wrong");
    check(roster->get(longId)->status=="killed"&&terminated==1,"TaskStop did not kill process");
    shell->cleanup(longId);
    auto abortRun=shell->run(shell::RunOptions{.command="sleep 20",.runInBackground=true});
    const auto abortId=*abortRun.backgroundMissionId;roster->registerShell(mission(abortId,"sleep 20"));
    std::stop_source stop;gadgets::GadgetCallContext abortedContext{Json::object(),stop.get_token(),{}};
    auto blocked=outputGadget.call(Json{{"task_id",abortId},{"timeout",5000}},abortedContext,{},Json(),{});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));stop.request_stop();
    bool abortError=false;try{(void)blocked.get();}catch(const shell_missions::MissionAbortError&){abortError=true;}
    check(abortError,"TaskOutput wait did not honor cancellation");
    roster->stop(abortId);shell->cleanup(abortId);
    shell::ExecResult early;early.code=0;
    roster->completeShell("early",early);roster->registerShell(mission("early","echo early"));
    check(roster->get("early")->status=="completed","early completion race lost result");
    std::promise<std::string> foregroundReady;
    std::atomic_bool registered{false};
    auto foreground=std::async(std::launch::async,[&]{
      shell::RunOptions options;options.command="sleep 3; echo foreground complete";
      options.onProgress=[&](const shell::Progress& progress){
        if(!registered.exchange(true)){
          roster->registerForeground(mission(progress.missionId,options.command));
          foregroundReady.set_value(progress.missionId);
        }
      };
      return shell->run(options);
    });
    auto foregroundIdFuture=foregroundReady.get_future();
    check(foregroundIdFuture.wait_for(std::chrono::seconds(4))==std::future_status::ready,
      "foreground registration was not reached");
    const auto foregroundId=foregroundIdFuture.get();
    check(roster->get(foregroundId)&&!roster->get(foregroundId)->isBackgrounded,
      "foreground task registration state wrong");
    const auto moved=roster->backgroundAllForeground();
    check(moved.size()==1&&moved.front()==foregroundId,
      "foreground task was not backgrounded in place");
    check(roster->get(foregroundId)->isBackgrounded,
      "background transition did not update task state");
    const auto backgroundResult=foreground.get();
    check(backgroundResult.backgroundMissionId==foregroundId,
      "background transition did not return the original task ID");
    check(shell->wait(foregroundId,3000).stdoutText.find("foreground complete")!=std::string::npos,
      "backgrounded foreground process lost output");
    check(roster->wait(foregroundId,3000,{})->status=="completed",
      "backgrounded foreground task completion lost");
    shell->cleanup(foregroundId);
    std::string ordinaryId;
    shell::RunOptions ordinaryOptions;ordinaryOptions.command="sleep 2.2; echo ordinary";
    ordinaryOptions.onProgress=[&](const shell::Progress& progress){
      if(ordinaryId.empty()){
        ordinaryId=progress.missionId;
        roster->registerForeground(mission(ordinaryId,ordinaryOptions.command));
      }
    };
    const auto ordinary=shell->run(ordinaryOptions);
    check(!ordinary.backgroundMissionId&&ordinary.stdoutText.find("ordinary")!=std::string::npos,
      "ordinary foreground command failed");
    check(!ordinaryId.empty(),"ordinary foreground registration missing");
    check(roster->unregisterForeground(ordinaryId),
      "foreground unregister did not remove task");
    check(!roster->get(ordinaryId),"foreground task remained registered");
    std::filesystem::remove_all(root);
    std::cout<<"PASS task registry/output/stop/cancellation/completion race/foreground backgrounding\n";
    return 0;
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;
  }
}
