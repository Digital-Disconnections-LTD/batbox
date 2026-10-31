#include "runtime/AsyncLaunch.hpp"
#include "gadgets/mission_output/MissionOutput.hpp"
#include <future>

namespace batbox::mission_output {
namespace {
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();
}
bool blockValue(const Json& input){
  return !input.contains("block")||input["block"].get<bool>();
}
double timeoutValue(const Json& input){
  return input.contains("timeout")?input["timeout"].get<double>():30000;
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
    if(timeout<0||timeout>600000)throw std::invalid_argument("TaskOutput timeout out of range");
  }else result["timeout"]=30000;
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
      {"timeout",{{"type","number"},{"minimum",0},{"maximum",600000},{"default",30000}}}}}};
  gadget.parseInput=parseInput;
  gadget.userFacingName=[](const Json&){return std::string("Task Output");};
  gadget.description=[](const Json&,const Json&){return ready(std::string(
    "[Deprecated] — prefer Read on the task output file path"));};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.isConcurrencySafe=gadget.isReadOnly;
  gadget.isEnabled=[shared]{return shared->enabled;};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{return input.value("task_id",std::string());};
  gadget.poetry=[](const gadgets::PoetryContext&){return ready(std::string(
    "DEPRECATED: Prefer using the Read tool on the task's output file path instead. Background tasks return their output file path in the tool result, and you receive a <task-notification> with the same path when the task completes — Read that file directly.\n\n"
    "- Retrieves output from a running or completed task (background shell, agent, or remote session)\n"
    "- Takes a task_id parameter identifying the task\n"
    "- Returns the task output along with status information\n"
    "- Use block=true (default) to wait for task completion\n"
    "- Use block=false for non-blocking check of current status\n"
    "- Task IDs can be found using the /tasks command\n"
    "- Works with all task types: background shells, async agents, and remote sessions"));};
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
        const auto completed=shared->roster->wait(id,timeoutValue(input),context.stopToken);
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
