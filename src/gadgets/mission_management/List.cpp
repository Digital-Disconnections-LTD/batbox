#include "gadgets/mission_management/GadgetSupport.hpp"
#include "gadgets/mission_management/PoetryEntries.hpp"
#include <set>

namespace batbox::mission_management {
namespace {
Json parse(const Json& input){detail::checkFields(input,"TaskList",{});return input;}
}
gadgets::Gadget makeListGadget(std::shared_ptr<Services> services){
  auto gadget=detail::base(services,"TaskList","list all tasks",
    "List all tasks in the task list",listPoetry);
  gadget.inputJSONSchema={{"type","object"},{"additionalProperties",false},
    {"properties",Json::object()}};
  gadget.outputSchema={{"type","object"},{"properties",{{"tasks",{{"type","array"},
    {"items",{{"type","object"}}}}}}}};
  gadget.parseInput=parse;gadget.isReadOnly=[](const Json&){return true;};
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    const auto& missions=data.at("tasks");
    if(missions.empty())return detail::resultBlock("No tasks found",id);
    std::vector<std::string> lines;
    for(const auto& mission:missions){
      std::string row="#"+detail::string(mission,"id")+" ["+
        detail::string(mission,"status")+"] "+detail::string(mission,"subject");
      const auto owner=detail::string(mission,"owner");
      if(!owner.empty())row+=" ("+owner+")";
      const auto blocked=detail::strings(mission.at("blockedBy"));
      if(!blocked.empty()){
        std::vector<std::string> refs;
        for(const auto& value:blocked)refs.push_back("#"+value);
        row+=" [blocked by "+detail::join(refs,", ")+"]";
      }
      lines.push_back(std::move(row));
    }
    return detail::resultBlock(detail::join(lines,"\n"),id);
  };
  gadget.call=[services](const Json&,const gadgets::GadgetCallContext&,
      const std::function<std::future<Json>(const Json&)>&,const Json&,
      const std::function<void(const gadgets::GadgetProgress&)>&){
    return runtime::contextAsync(std::launch::async,[services]{
      auto missions=services->store->list(detail::listId(*services));
      std::vector<Json> visible;
      std::set<std::string> resolved;
      for(const auto& mission:missions){
        const auto& metadata=value::property(mission,"metadata");
        if(metadata.is_object()&&value::truthy(value::property(metadata,"_internal")))continue;
        if(mission.at("status")=="completed")resolved.insert(detail::string(mission,"id"));
        visible.push_back(mission);
      }
      Json rows=Json::array();
      for(const auto& mission:visible){
        Json blocked=Json::array();
        for(const auto& id:mission.at("blockedBy"))
          if(id.is_string()&&!resolved.contains(id.get<std::string>()))blocked.push_back(id);
        Json row={{"id",mission.at("id")},{"subject",mission.at("subject")},
          {"status",mission.at("status")},{"blockedBy",std::move(blocked)}};
        if(mission.contains("owner"))row["owner"]=mission.at("owner");
        rows.push_back(std::move(row));
      }
      gadgets::GadgetResult result;result.data={{"tasks",std::move(rows)}};
      return result;
    });
  };
  return gadgets::buildGadget(std::move(gadget));
}
std::vector<gadgets::Gadget> makeMissionGadgets(Services services){
  if(!services.store||!services.missionListId)
    throw std::invalid_argument("Task store and list ID provider required");
  auto shared=std::make_shared<Services>(std::move(services));
  std::vector<gadgets::Gadget> result;
  result.reserve(4);
  result.push_back(makeCreateGadget(shared));
  result.push_back(makeGetGadget(shared));
  result.push_back(makeUpdateGadget(shared));
  result.push_back(makeListGadget(shared));
  return result;
}
} // namespace batbox::mission_management
