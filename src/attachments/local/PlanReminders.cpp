#include "attachments/local/LocalCollectors.hpp"
#include <stdexcept>

namespace batbox::attachments::local {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
bool gadgetResultContent(const Json& content){
  if(!content.is_array())return false;
  for(const auto& block:content)
    if(field(block,"type")=="tool_result"&&field(block,"tool_use_id").is_string())
      return true;
  return false;
}
bool humanMessage(const Json& message){
  return field(message,"type")=="user"&&
    !value::truthy(field(message,"isMeta"))&&
    !gadgetResultContent(field(field(message,"message"),"content"));
}
bool humanTurn(const Json& message){
  return field(message,"type")=="user"&&
    !value::truthy(field(message,"isMeta"))&&
    (!message.is_object()||!message.contains("toolUseResult"));
}
std::optional<std::string> batId(const execution::Context& context){
  const auto& value=field(context.data,"agentId");
  if(!value::truthy(value))return std::nullopt;
  return value.get<std::string>();
}
std::string mode(const app_state::AppStateRef& app){
  if(!app||!app->permissionContext)
    throw std::invalid_argument("Live permission context required");
  const auto value=app->permissionContext->get("mode");
  return value&&value->is_string()?value->get<std::string>():std::string();
}
app_state::AppStateRef current(const Services& services){
  if(!services.getAppState)
    throw std::invalid_argument("Live AppState import required");
  return services.getAppState();
}
std::pair<int,bool> turnCount(const Json& messages){
  int count=0;bool found=false;
  if(!messages.is_array())return {count,found};
  for(std::size_t i=messages.size();i>0;){
    const auto& message=messages[--i];
    const auto& type=field(message,"type");
    if(humanMessage(message))++count;
    else if(type=="attachment"){
      const auto& attachmentType=field(field(message,"attachment"),"type");
      if(attachmentType=="plan_mode"||attachmentType=="plan_mode_reentry"){
        found=true;break;
      }
    }
  }
  return {count,found};
}
int countSinceExit(const Json& messages){
  int count=0;
  if(!messages.is_array())return count;
  for(std::size_t i=messages.size();i>0;){
    const auto& message=messages[--i];
    if(field(message,"type")!="attachment")continue;
    const auto& type=field(field(message,"attachment"),"type");
    if(type=="plan_mode_exit")break;
    if(type=="plan_mode")++count;
  }
  return count;
}
}
Json getPlanModeAttachments(const Json& messages,
    const execution::Context& context,const Services& services){
  if(mode(current(services))!="plan")return Json::array();
  if(messages.is_array()&&!messages.empty()){
    const auto [turns,found]=turnCount(messages);
    if(found&&turns<5)return Json::array();
  }
  if(!services.planFilePath||!services.getPlan||!services.transitions)
    throw std::invalid_argument("Plan path, storage and transition imports required");
  const auto id=batId(context);
  const auto path=services.planFilePath(id);
  const auto plan=services.getPlan(id);
  const bool exists=plan&&!plan->is_null();
  Json attachments=Json::array();
  if(bootstrap::hasExitedPlanModeInFlight(*services.transitions)&&exists){
    attachments.push_back(Json{{"type","plan_mode_reentry"},
      {"planFilePath",path}});
    bootstrap::setHasExitedPlanMode(*services.transitions,false);
  }
  const auto reminder=((countSinceExit(messages)+1)%5)==1?"full":"sparse";
  attachments.push_back(Json{{"type","plan_mode"},
    {"reminderType",reminder},{"isSubAgent",id.has_value()},
    {"planFilePath",path},{"planExists",exists}});
  return attachments;
}
Json getPlanModeExitAttachment(const execution::Context& context,
    const Services& services){
  if(!services.transitions)
    throw std::invalid_argument("Plan transition state required");
  if(!bootstrap::needsPlanModeExitAttachment(*services.transitions))
    return Json::array();
  if(mode(current(services))=="plan"){
    bootstrap::setNeedsPlanModeExitAttachment(*services.transitions,false);
    return Json::array();
  }
  bootstrap::setNeedsPlanModeExitAttachment(*services.transitions,false);
  if(!services.planFilePath||!services.getPlan)
    throw std::invalid_argument("Plan path and storage imports required");
  const auto id=batId(context);
  const auto path=services.planFilePath(id);
  const auto plan=services.getPlan(id);
  return Json::array({Json{{"type","plan_mode_exit"},
    {"planFilePath",path},{"planExists",plan&&!plan->is_null()}}});
}
Json getVerifyPlanReminderAttachment(const Json& messages,
    const Services& services){
  if(!services.internalUser)
    throw std::invalid_argument("Local user type import required");
  if(!services.internalUser())return Json::array();
  if(!services.verifyPlanEnabled)
    throw std::invalid_argument("Plan verification feature import required");
  if(!services.verifyPlanEnabled())return Json::array();
  const auto app=current(services);
  const auto pending=app->fieldRef("pendingPlanVerification");
  if(!pending||!value::truthy(*pending)||
     value::truthy(field(*pending,"verificationStarted"))||
     value::truthy(field(*pending,"verificationCompleted")))
    return Json::array();
  if(messages.is_array()&&!messages.empty()){
    int count=0;bool found=false;
    for(std::size_t i=messages.size();i>0;){
      const auto& message=messages[--i];
      if(humanTurn(message))++count;
      if(field(message,"type")=="attachment"&&
         field(field(message,"attachment"),"type")=="plan_mode_exit"){
        found=true;break;
      }
    }
    if(!found||count==0||count%10!=0)return Json::array();
  }
  return Json::array({Json{{"type","verify_plan_reminder"}}});
}
} // namespace batbox::attachments::local
