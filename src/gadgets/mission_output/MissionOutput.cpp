#include "runtime/AsyncLaunch.hpp"
#include "gadgets/mission_output/MissionOutput.hpp"
#include <future>

namespace batbox::mission_output {
namespace {
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();
}
// A Bat on a slow backend can run for an hour; a background shell may be a
// server that never ends, so its default wait stays short.
constexpr double maxWaitMs=3600000;
constexpr double shellWaitMs=30000;
bool blockValue(const Json& input){
  return !input.contains("block")||input["block"].get<bool>();
}
double timeoutValue(const Json& input,const shell_missions::MissionRecord& mission){
  if(input.contains("timeout"))return input["timeout"].get<double>();
  return mission.type=="local_bash"?shellWaitMs:maxWaitMs;
}
}
Json parseInput(const Json& input){
  if(!input.is_object())throw std::invalid_argument("TaskOutput input must be an object");
  for(auto it=input.begin();it!=input.end();++it)
    if(it.key()!="task_id"&&it.key()!="block"&&it.key()!="timeout")
      throw std::invalid_argument("Unrecognized TaskOutput input field: "+it.key());
  if(!input.contains("task_id")||!input["task_id"].is_string())
    throw std::invalid_argument("TaskOutput task_id must be a string");
  Json result=input;
  if(result.contains("block")){
    if(result["block"].is_string()){
      const auto text=result["block"].get<std::string>();
      if(text=="true")result["block"]=true;
      else if(text=="false")result["block"]=false;
    }
    if(!result["block"].is_boolean())throw std::invalid_argument("TaskOutput block must be a boolean");
  }else result["block"]=true;
  if(result.contains("timeout")){
    if(!result["timeout"].is_number())throw std::invalid_argument("TaskOutput timeout must be a number");
    const auto timeout=result["timeout"].get<double>();
    if(timeout<0||timeout>maxWaitMs)throw std::invalid_argument("TaskOutput timeout out of range");
  }
  return result;
}
gadgets::Gadget makeMissionOutputGadget(Services services){
  if(!services.roster||!services.nowMs||!services.getMissionOutputPath||!services.renderResult||
     !services.renderProgress||!services.renderError||!services.renderRejected||!services.renderTag)
    throw std::invalid_argument("TaskOutput registry, clock, path, and UI services required");
  auto shared=std::make_shared<Services>(std::move(services));
  gadgets::Gadget gadget;gadget.name="TaskOutput";gadget.searchHint="read output/logs from a background task";
  gadget.aliases={"AgentOutputTool","BashOutputTool"};gadget.maxResultSizeChars=100000;
  gadget.shouldDefer=true;
  gadget.inputJSONSchema={{"type","object"},{"additionalProperties",false},
    {"required",Json::array({"task_id"})},
    {"properties",{{"task_id",{{"type","string"}}},
      {"block",{{"type","boolean"},{"default",true}}},
      {"timeout",{{"type","number"},{"minimum",0},{"maximum",maxWaitMs},
        {"description","Milliseconds to wait when block is true. Defaults to "
          "3600000 (one hour) for an agent and 30000 for a background shell command."}}}}}};
  gadget.parseInput=parseInput;
  gadget.userFacingName=[](const Json&){return std::string("Task Output");};
  gadget.description=[](const Json&,const Json&){return ready(std::string(
    "Wait for a background task's result"));};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.isConcurrencySafe=gadget.isReadOnly;
  gadget.isEnabled=[shared]{return shared->enabled;};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{return input.value("task_id",std::string());};
  gadget.poetry=[](const gadgets::PoetryContext&){return ready(std::string(
    "Waits for a background agent or background shell command and returns its result.\n\n"
    "- Takes a task_id: an agent's agentId or a background command's ID\n"
    "- block=true (default) waits until the task finishes, up to timeout. Use it when your next step depends on a background agent's result, instead of launching the agent again\n"
    "- The default wait is one hour for an agent and 30 seconds for a background shell command, which may be a server that never exits\n"
    "- If the wait ends first, the task is still running, not failed; call again to keep waiting\n"
    "- block=false checks the current status without waiting\n"
    "- Finished background tasks also arrive as a <task-notification>; you do not need to poll them"));};
  gadget.validateInput=[shared](const Json& input,const Json&){
    gadgets::ValidationResult validation;
    const auto id=input.value("task_id",std::string());
    if(id.empty()){validation.result=false;validation.message="Task ID is required";
      validation.errorCode=1;return ready(validation);}
    if(!shared->roster->get(id)){validation.result=false;
      validation.message="No task found with ID: "+id;validation.errorCode=2;}
    return ready(validation);
  };
  gadget.call=[shared](const Json& input,const gadgets::GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const gadgets::GadgetProgress&)>& onProgress){
    return ::batbox::runtime::contextAsync(std::launch::async,[shared,input,context,onProgress]{
      const auto id=input.at("task_id").get<std::string>();
      const auto mission=shared->roster->get(id);
      if(!mission)throw std::runtime_error("No task found with ID: "+id);
      Json data;
      if(!blockValue(input)){
        if(mission->status!="running"&&mission->status!="pending"){
          shared->roster->markNotified(id);
          data={{"retrieval_status","success"},{"task",getMissionOutputData(*mission,*shared->roster)}};
        }else data={{"retrieval_status","not_ready"},{"task",getMissionOutputData(*mission,*shared->roster)}};
      }else{
        if(onProgress)onProgress({"task-output-waiting-"+std::to_string(shared->nowMs()),
          {{"type","waiting_for_task"},{"taskDescription",mission->description},{"taskType",mission->type}}});
        const auto completed=shared->roster->wait(id,timeoutValue(input,*mission),context.stopToken);
        if(!completed)data={{"retrieval_status","timeout"},{"task",nullptr}};
        else if(completed->status=="running"||completed->status=="pending")
          data={{"retrieval_status","timeout"},{"task",getMissionOutputData(*completed,*shared->roster)}};
        else{
          shared->roster->markNotified(id);
          data={{"retrieval_status","success"},{"task",getMissionOutputData(*completed,*shared->roster)}};
        }
      }
      gadgets::GadgetResult output;output.data=std::move(data);return output;
    });
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[shared](const Json& data,const std::string& id){
    return mapGadgetResult(data,id,shared->maxOutputLength,shared->getMissionOutputPath);};
  gadget.renderGadgetUseMessage=[](const Json& input,const Json&)->Json{
    return input.is_object()&&input.contains("block")&&input["block"]==false?"non-blocking":"";};
  gadget.renderGadgetUseTag=shared->renderTag;
  gadget.renderGadgetUseProgressMessage=shared->renderProgress;
  gadget.renderGadgetResultMessage=shared->renderResult;
  gadget.renderGadgetUseRejectedMessage=shared->renderRejected;
  gadget.renderGadgetUseErrorMessage=shared->renderError;
  return gadgets::buildGadget(std::move(gadget));
}
} // namespace batbox::mission_output
