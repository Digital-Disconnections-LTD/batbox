#include "attachments/local/LocalCollectors.hpp"
#include <stdexcept>

namespace batbox::attachments::local {
namespace {
const Json& field(const Json& value,std::string_view key){return value::property(value,key);}
bool available(const execution::Context& context,const std::string& name){
  return !name.empty()&&gadgets::findGadgetByName(context.gadgets,name)!=nullptr;
}
bool thinking(const Json& message){
  if(field(message,"type")!="assistant")return false;
  const auto& content=field(field(message,"message"),"content");
  if(!content.is_array())return false;
  for(const auto& block:content){
    const auto& type=field(block,"type");
    if(type!="thinking"&&type!="redacted_thinking")return false;
  }
  return true;
}
bool usedGadget(const Json& message,const std::string& first,
    const std::string& second={}){
  const auto& content=field(field(message,"message"),"content");
  if(!content.is_array())return false;
  for(const auto& block:content)
    if(field(block,"type")=="tool_use"&&
       (field(block,"name")==first||
        (!second.empty()&&field(block,"name")==second)))return true;
  return false;
}
std::pair<int,int> counts(const Json& messages,const std::string& reminder,
    const std::string& firstGadget,const std::string& secondGadget={}){
  int sinceGadget=0,sinceReminder=0;
  bool foundGadget=false,foundReminder=false;
  for(std::size_t i=messages.size();i>0;){
    const auto& message=messages[--i];
    if(field(message,"type")=="assistant"){
      if(thinking(message))continue;
      if(!foundGadget&&usedGadget(message,firstGadget,secondGadget))foundGadget=true;
      if(!foundGadget)++sinceGadget;
      if(!foundReminder)++sinceReminder;
    }else if(!foundReminder&&field(message,"type")=="attachment"&&
        field(field(message,"attachment"),"type")==reminder){
      foundReminder=true;
    }
    if(foundGadget&&foundReminder)break;
  }
  return {sinceGadget,sinceReminder};
}
event_loop::Deferred<Json> ready(event_loop::Loop& loop,Json value){
  event_loop::Deferred<Json> output(loop);output.resolve(std::move(value));
  return output;
}
}
Json getTodoReminderAttachments(const Json& messages,
    const execution::Context& context,const Services& services){
  if(!available(context,services.todoWriteGadgetName)||
     available(context,services.briefGadgetName)||
     !messages.is_array()||messages.empty())return Json::array();
  const auto [sinceWrite,sinceReminder]=counts(messages,"todo_reminder",
    services.todoWriteGadgetName);
  if(sinceWrite<10||sinceReminder<10)return Json::array();
  const auto& bat=field(context.data,"agentId");
  std::string key;
  if(value::truthy(bat))key=bat.get<std::string>();
  else{
    if(!services.flightId)
      throw std::invalid_argument("Session identity import required");
    key=services.flightId();
  }
  if(!services.getAppState)
    throw std::invalid_argument("Live AppState import required");
  const auto app=services.getAppState();
  if(!app||!app->fields)
    throw std::invalid_argument("Typed todo state required");
  const auto todos=app->fieldRef("todos");
  const auto& list=todos?field(*todos,key):Json();
  const Json content=list.is_array()?list:Json::array();
  return Json::array({Json{{"type","todo_reminder"},
    {"content",content},{"itemCount",content.size()}}});
}
event_loop::Deferred<Json> getMissionReminderAttachments(const Json& messages,
    const execution::Context& context,const Services& services){
  if(!services.todoV2Enabled)
    throw std::invalid_argument("Task reminder feature import required");
  if(!services.todoV2Enabled())return ready(services.loop,Json::array());
  if(!services.internalUser)
    throw std::invalid_argument("Local user type import required");
  if(services.internalUser()||
     available(context,services.briefGadgetName)||
     !available(context,services.missionUpdateGadgetName)||
     !messages.is_array()||messages.empty())
    return ready(services.loop,Json::array());
  const auto [sinceManagement,sinceReminder]=counts(messages,"task_reminder",
    services.missionCreateGadgetName,services.missionUpdateGadgetName);
  if(sinceManagement<10||sinceReminder<10)
    return ready(services.loop,Json::array());
  event_loop::Deferred<Json> output(services.loop);
  try{
    if(!services.missionListId||!services.listMissions)
      throw std::invalid_argument("Task list imports required");
    services.listMissions(services.missionListId()).then(
      [output](const Json* missions,const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        if(!missions||!missions->is_array()){
          output.reject(std::make_exception_ptr(std::invalid_argument(
            "Task list must be an array")));return;
        }
        output.resolve(Json::array({Json{{"type","task_reminder"},
          {"content",*missions},{"itemCount",missions->size()}}}));
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::attachments::local
