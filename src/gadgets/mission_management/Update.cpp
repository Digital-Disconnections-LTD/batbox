#include "gadgets/mission_management/GadgetSupport.hpp"
#include "gadgets/mission_management/PoetryEntries.hpp"
#include <algorithm>

namespace batbox::mission_management {
namespace {
Json parse(const Json& input){
  detail::checkFields(input,"TaskUpdate",{"taskId","subject","description",
    "activeForm","status","addBlocks","addBlockedBy","owner","metadata"});
  detail::stringField(input,"taskId",true);
  for(const char* key:{"subject","description","activeForm","status","owner"})
    detail::stringField(input,key);
  detail::stringArray(input,"addBlocks");detail::stringArray(input,"addBlockedBy");
  detail::metadata(input);
  if(input.contains("status")){
    const auto status=input.at("status").get<std::string>();
    if(status!="pending"&&status!="in_progress"&&status!="completed"&&
        status!="deleted")throw std::invalid_argument("Invalid task status");
  }
  return input;
}
Json dataFailure(const std::string& id,const std::string& error){
  return {{"success",false},{"taskId",id},{"updatedFields",Json::array()},
    {"error",error}};
}
void maybeUpdate(const Json& input,const Json& existing,Json& updates,
    std::vector<std::string>& changed,const char* key){
  if(input.contains(key)&&(!existing.contains(key)||existing.at(key)!=input.at(key))){
    updates[key]=input.at(key);changed.push_back(key);
  }
}
Json updateData(const Services& services,const Json& input,
    const gadgets::GadgetCallContext& context){
  const auto list=detail::listId(services);
  const auto id=input.at("taskId").get<std::string>();
  if(services.expandMissions)services.expandMissions();
  const auto existing=services.store->get(list,id);
  if(!existing)return dataFailure(id,"Task not found");
  Json updates=Json::object();std::vector<std::string> changed;
  for(const char* key:{"subject","description","activeForm","owner"})
    maybeUpdate(input,*existing,updates,changed,key);
  if(input.contains("metadata")){
    Json merged=existing->value("metadata",Json::object());
    if(!merged.is_object())merged=Json::object();
    for(auto it=input.at("metadata").begin();it!=input.at("metadata").end();++it){
      if(it.value().is_null())merged.erase(it.key());else merged[it.key()]=it.value();
    }
    updates["metadata"]=std::move(merged);changed.push_back("metadata");
  }
  std::optional<Json> statusChange;
  if(input.contains("status")){
    const auto status=input.at("status").get<std::string>();
    if(status=="deleted"){
      const bool deleted=services.store->erase(list,id);
      Json result={{"success",deleted},{"taskId",id},
        {"updatedFields",deleted?Json::array({"deleted"}):Json::array()}};
      if(deleted)result["statusChange"]={{"from",existing->at("status")},{"to","deleted"}};
      else result["error"]="Failed to delete task";
      return result;
    }
    if(existing->at("status")!=status){
      if(status=="completed"&&services.lifecycleHooks){
        const Json event={{"task_id",id},{"task_subject",existing->at("subject")},
          {"task_description",existing->at("description")}};
        const auto blockers=services.lifecycleHooks("TaskCompleted",event,context);
        if(!blockers.empty())return dataFailure(id,detail::join(blockers,"\n"));
      }
      updates["status"]=status;changed.push_back("status");
      statusChange=Json{{"from",existing->at("status")},{"to",status}};
    }
  }
  if(!updates.empty())services.store->update(list,id,updates);
  for(const auto& blockId:detail::strings(input.value("addBlocks",Json::array()))){
    const auto& blocks=existing->at("blocks");
    if(std::find(blocks.begin(),blocks.end(),blockId)==blocks.end()){
      services.store->block(list,id,blockId);
      if(std::find(changed.begin(),changed.end(),"blocks")==changed.end())
        changed.push_back("blocks");
    }
  }
  for(const auto& blockerId:detail::strings(input.value("addBlockedBy",Json::array()))){
    const auto& blocked=existing->at("blockedBy");
    if(std::find(blocked.begin(),blocked.end(),blockerId)==blocked.end()){
      services.store->block(list,blockerId,id);
      if(std::find(changed.begin(),changed.end(),"blockedBy")==changed.end())
        changed.push_back("blockedBy");
    }
  }
  Json output={{"success",true},{"taskId",id},{"updatedFields",changed},
    {"verificationNudgeNeeded",false}};
  if(statusChange)output["statusChange"]=*statusChange;
  return output;
}
}
gadgets::Gadget makeUpdateGadget(std::shared_ptr<Services> services){
  auto gadget=detail::base(services,"TaskUpdate","update a task",
    "Update a task in the task list",updatePoetry);
  Json fields={{"taskId",{{"type","string"}}}};
  for(const char* key:{"subject","description","activeForm","owner"})
    fields[key]={{"type","string"}};
  fields["status"]={{"type","string"},
    {"enum",Json::array({"pending","in_progress","completed","deleted"})}};
  for(const char* key:{"addBlocks","addBlockedBy"})
    fields[key]={{"type","array"},{"items",{{"type","string"}}}};
  fields["metadata"]={{"type","object"},{"additionalProperties",true}};
  gadget.inputJSONSchema={{"type","object"},{"additionalProperties",false},
    {"required",Json::array({"taskId"})},{"properties",fields}};
  gadget.outputSchema={{"type","object"},{"properties",{{"success",{{"type","boolean"}}},
    {"taskId",{{"type","string"}}},{"updatedFields",{{"type","array"},
      {"items",{{"type","string"}}}}}}}};
  gadget.parseInput=parse;
  gadget.toAutoClassifierInput=[](const Json& input)->Json{
    std::vector<std::string> parts={detail::string(input,"taskId")};
    for(const char* key:{"status","subject"}){
      const auto value=detail::string(input,key);if(!value.empty())parts.push_back(value);
    }
    return detail::join(parts," ");
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    if(!data.at("success").get<bool>())return detail::resultBlock(
      detail::string(data,"error").empty()?"Task #"+detail::string(data,"taskId")+
        " not found":detail::string(data,"error"),id);
    return detail::resultBlock("Updated task #"+detail::string(data,"taskId")+" "+
      detail::join(detail::strings(data.at("updatedFields")),", "),id);
  };
  gadget.call=[services](const Json& input,const gadgets::GadgetCallContext& context,
      const std::function<std::future<Json>(const Json&)>&,const Json&,
      const std::function<void(const gadgets::GadgetProgress&)>&){
    return runtime::contextAsync(std::launch::async,[services,input,context]{
      gadgets::GadgetResult result;result.data=updateData(*services,input,context);
      return result;
    });
  };
  return gadgets::buildGadget(std::move(gadget));
}
} // namespace batbox::mission_management
