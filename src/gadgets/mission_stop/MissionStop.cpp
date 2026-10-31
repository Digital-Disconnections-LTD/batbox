#include "runtime/AsyncLaunch.hpp"
#include "gadgets/mission_stop/MissionStop.hpp"
#include <future>

namespace batbox::mission_stop {
namespace {
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();
}
std::optional<std::string> chosenId(const Json& input){
  if(input.is_object()&&input.contains("task_id")&&!input["task_id"].is_null())
    return input["task_id"].get<std::string>();
  if(input.is_object()&&input.contains("shell_id")&&!input["shell_id"].is_null())
    return input["shell_id"].get<std::string>();
  return std::nullopt;
}
}
Json parseInput(const Json& input){
  if(!input.is_object())throw std::invalid_argument("TaskStop input must be an object");
  for(auto it=input.begin();it!=input.end();++it){
    if(it.key()!="task_id"&&it.key()!="shell_id")throw std::invalid_argument("Unrecognized TaskStop input field: "+it.key());
    if(!it.value().is_string())throw std::invalid_argument("TaskStop "+it.key()+" must be a string");
  }
  return input;
}
gadgets::Gadget makeMissionStopGadget(Services services){
  if(!services.roster||!services.renderUse||!services.renderResult)
    throw std::invalid_argument("TaskStop registry and render services required");
  auto shared=std::make_shared<Services>(std::move(services));
  gadgets::Gadget gadget;gadget.name="TaskStop";gadget.searchHint="kill a running background task";
  gadget.aliases={"KillShell"};gadget.maxResultSizeChars=100000;gadget.shouldDefer=true;
  gadget.inputJSONSchema={{"type","object"},{"additionalProperties",false},
    {"properties",{{"task_id",{{"type","string"}}},{"shell_id",{{"type","string"}}}}}};
  gadget.outputSchema={{"type","object"},{"properties",{{"message",{{"type","string"}}},
    {"task_id",{{"type","string"}}},{"task_type",{{"type","string"}}},
    {"command",{{"type","string"}}}}}};
  gadget.parseInput=parseInput;
  gadget.userFacingName=[shared](const Json&){return shared->hideUserFacingName?"":"Stop Task";};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{
    const auto id=chosenId(input);return id?Json(*id):Json("");};
  gadget.validateInput=[shared](const Json& input,const Json&){
    gadgets::ValidationResult result;
    const auto id=chosenId(input);
    if(!id||id->empty()){result.result=false;result.message="Missing required parameter: task_id";
      result.errorCode=1;return ready(result);}
    const auto mission=shared->roster->get(*id);
    if(!mission){result.result=false;result.message="No task found with ID: "+*id;
      result.errorCode=1;return ready(result);}
    if(mission->status!="running"){
      result.result=false;result.message="Task "+*id+" is not running (status: "+mission->status+")";
      result.errorCode=3;return ready(result);
    }
    return ready(result);
  };
  gadget.description=[](const Json&,const Json&){return ready(std::string("Stop a running background task by ID"));};
  gadget.poetry=[](const gadgets::PoetryContext&){return ready(std::string(
    "\n- Stops a running background task by its ID\n"
    "- Takes a task_id parameter identifying the task to stop\n"
    "- Returns a success or failure status\n"
    "- Use this tool when you need to terminate a long-running task\n"));};
  gadget.mapGadgetResultToGadgetResultBlockParam=mapGadgetResult;
  gadget.renderGadgetUseMessage=shared->renderUse;gadget.renderGadgetResultMessage=shared->renderResult;
  gadget.call=[shared](const Json& input,const gadgets::GadgetCallContext&,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const gadgets::GadgetProgress&)>&){
    return ::batbox::runtime::contextAsync(std::launch::async,[shared,input]{
      const auto id=chosenId(input);
      if(!id||id->empty())throw std::runtime_error("Missing required parameter: task_id");
      const auto result=shared->roster->stop(*id);
      const auto command=result.command.value_or("undefined");
      Json data={{"message","Successfully stopped task: "+result.missionId+" ("+command+")"},
        {"task_id",result.missionId},{"task_type",result.missionType}};
      if(result.command)data["command"]=*result.command;
      gadgets::GadgetResult output;output.data=std::move(data);return output;
    });
  };
  return gadgets::buildGadget(std::move(gadget));
}
} // namespace batbox::mission_stop
