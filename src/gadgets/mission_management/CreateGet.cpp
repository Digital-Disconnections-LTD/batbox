#include "gadgets/mission_management/GadgetSupport.hpp"
#include "gadgets/mission_management/PoetryEntries.hpp"

namespace batbox::mission_management {
namespace {
Json parseCreate(const Json& input){
  detail::checkFields(input,"TaskCreate",{"subject","description","activeForm","metadata"});
  detail::stringField(input,"subject",true);
  detail::stringField(input,"description",true);
  detail::stringField(input,"activeForm");detail::metadata(input);
  return input;
}
Json parseGet(const Json& input){
  detail::checkFields(input,"TaskGet",{"taskId"});
  detail::stringField(input,"taskId",true);return input;
}
}
gadgets::Gadget makeCreateGadget(std::shared_ptr<Services> services){
  auto gadget=detail::base(services,"TaskCreate","create a task in the task list",
    "Create a new task in the task list",createPoetry);
  gadget.inputJSONSchema={{"type","object"},{"additionalProperties",false},
    {"required",Json::array({"subject","description"})},
    {"properties",{{"subject",{{"type","string"},{"description","A brief title for the task"}}},
      {"description",{{"type","string"},{"description","What needs to be done"}}},
      {"activeForm",{{"type","string"},{"description","Present continuous form shown in spinner when in_progress"}}},
      {"metadata",{{"type","object"},{"additionalProperties",true}}}}}};
  gadget.outputSchema={{"type","object"},{"properties",{{"task",{{"type","object"},
    {"properties",{{"id",{{"type","string"}}},{"subject",{{"type","string"}}}}}}}}}};
  gadget.parseInput=parseCreate;
  gadget.toAutoClassifierInput=[](const Json& input)->Json{return input.at("subject");};
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    const auto& mission=data.at("task");
    return detail::resultBlock("Task #"+detail::string(mission,"id")+
      " created successfully: "+detail::string(mission,"subject"),id);};
  gadget.call=[services](const Json& input,const gadgets::GadgetCallContext& context,
      const std::function<std::future<Json>(const Json&)>&,const Json&,
      const std::function<void(const gadgets::GadgetProgress&)>&){
    return runtime::contextAsync(std::launch::async,[services,input,context]{
      const auto list=detail::listId(*services);
      Json mission={{"subject",input.at("subject")},{"description",input.at("description")},
        {"status","pending"},{"blocks",Json::array()},{"blockedBy",Json::array()}};
      for(const char* key:{"activeForm","metadata"})
        if(input.contains(key))mission[key]=input.at(key);
      const auto id=services->store->create(list,std::move(mission));
      if(services->lifecycleHooks){
        const Json event={{"task_id",id},{"task_subject",input.at("subject")},
          {"task_description",input.at("description")}};
        const auto blockers=services->lifecycleHooks("TaskCreated",event,context);
        if(!blockers.empty()){
          services->store->erase(list,id);
          throw std::runtime_error(detail::join(blockers,"\n"));
        }
      }
      if(services->expandMissions)services->expandMissions();
      gadgets::GadgetResult result;
      result.data={{"task",{{"id",id},{"subject",input.at("subject")}}}};
      return result;
    });
  };
  return gadgets::buildGadget(std::move(gadget));
}
gadgets::Gadget makeGetGadget(std::shared_ptr<Services> services){
  auto gadget=detail::base(services,"TaskGet","retrieve a task by ID",
    "Get a task by ID from the task list",getPoetry);
  gadget.inputJSONSchema={{"type","object"},{"additionalProperties",false},
    {"required",Json::array({"taskId"})},
    {"properties",{{"taskId",{{"type","string"},{"description","The ID of the task to retrieve"}}}}}};
  gadget.outputSchema={{"type","object"},{"properties",{{"task",{{"oneOf",Json::array({
    Json{{"type","null"}},Json{{"type","object"}}})}}}}}};
  gadget.parseInput=parseGet;gadget.isReadOnly=[](const Json&){return true;};
  gadget.toAutoClassifierInput=[](const Json& input)->Json{return input.at("taskId");};
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    const auto& mission=data.at("task");
    if(mission.is_null())return detail::resultBlock("Task not found",id);
    std::string content="Task #"+detail::string(mission,"id")+": "+
      detail::string(mission,"subject")+"\nStatus: "+detail::string(mission,"status")+
      "\nDescription: "+detail::string(mission,"description");
    const auto blockedBy=detail::strings(mission.at("blockedBy"));
    const auto blocks=detail::strings(mission.at("blocks"));
    if(!blockedBy.empty()){
      std::vector<std::string> refs;for(const auto& value:blockedBy)refs.push_back("#"+value);
      content+="\nBlocked by: "+detail::join(refs,", ");
    }
    if(!blocks.empty()){
      std::vector<std::string> refs;for(const auto& value:blocks)refs.push_back("#"+value);
      content+="\nBlocks: "+detail::join(refs,", ");
    }
    return detail::resultBlock(content,id);
  };
  gadget.call=[services](const Json& input,const gadgets::GadgetCallContext&,
      const std::function<std::future<Json>(const Json&)>&,const Json&,
      const std::function<void(const gadgets::GadgetProgress&)>&){
    return runtime::contextAsync(std::launch::async,[services,input]{
      const auto found=services->store->get(detail::listId(*services),
        input.at("taskId").get<std::string>());
      gadgets::GadgetResult result;result.data={{"task",nullptr}};
      if(found){const auto& mission=*found;
        result.data["task"]={{"id",mission.at("id")},{"subject",mission.at("subject")},
          {"description",mission.at("description")},{"status",mission.at("status")},
          {"blocks",mission.at("blocks")},{"blockedBy",mission.at("blockedBy")}};}
      return result;
    });
  };
  return gadgets::buildGadget(std::move(gadget));
}
} // namespace batbox::mission_management
