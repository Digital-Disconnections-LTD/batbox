#include "bats/BatMissionProgress.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
double number(const Json& value){return value.is_number()?value.get<double>():0.0;}
}
Json createProgressTracker(){
  return Json{{"toolUseCount",0},{"latestInputTokens",0},
    {"cumulativeOutputTokens",0},{"recentActivities",Json::array()}};
}
double getTokenCountFromTracker(const Json& tracker){
  return number(field(tracker,"latestInputTokens"))+
    number(field(tracker,"cumulativeOutputTokens"));
}
void updateProgressFromMessage(Json& tracker,const Json& message,
  const ActivityResolver& resolveActivityDescription,
  const gadgets::Gadgets& gadgets,BatMissionProgressImports& imports){
  if(field(message,"type")!="assistant")return;
  const Json& body=field(message,"message");
  const Json& usage=field(body,"usage");
  tracker["latestInputTokens"]=number(field(usage,"input_tokens"))+
    number(field(usage,"cache_creation_input_tokens"))+
    number(field(usage,"cache_read_input_tokens"));
  tracker["cumulativeOutputTokens"]=number(field(tracker,"cumulativeOutputTokens"))+
    number(field(usage,"output_tokens"));
  const Json& blocks=field(body,"content");
  if(blocks.is_array())for(const Json& block:blocks){
    if(field(block,"type")!="tool_use")continue;
    tracker["toolUseCount"]=number(field(tracker,"toolUseCount"))+1;
    if(field(block,"name")==imports.syntheticOutputGadgetName)continue;
    const Json& name=field(block,"name");
    if(!name.is_string())continue;
    const std::string gadgetName=name.get<std::string>();
    const Json& input=field(block,"input");
    if(!imports.getGadgetSearchOrReadInfo)
      throw std::invalid_argument("Unbound LocalAgentTask import: getToolSearchOrReadInfo");
    const Json classification=imports.getGadgetSearchOrReadInfo(gadgetName,input,gadgets);
    Json activity={{"toolName",gadgetName},{"input",input}};
    if(resolveActivityDescription){
      auto description=resolveActivityDescription(gadgetName,input);
      if(description)activity["activityDescription"]=*description;
    }
    const Json& isSearch=field(classification,"isSearch");
    const Json& isRead=field(classification,"isRead");
    if(!isSearch.is_null())activity["isSearch"]=isSearch;
    if(!isRead.is_null())activity["isRead"]=isRead;
    tracker["recentActivities"].push_back(std::move(activity));
  }
  while(tracker["recentActivities"].size()>5)
    tracker["recentActivities"].erase(tracker["recentActivities"].begin());
}
Json getProgressUpdate(const Json& tracker){
  Json out={{"toolUseCount",field(tracker,"toolUseCount")},
    {"tokenCount",getTokenCountFromTracker(tracker)},
    {"recentActivities",field(tracker,"recentActivities")}};
  const Json& activities=field(tracker,"recentActivities");
  if(activities.is_array()&&!activities.empty())out["lastActivity"]=activities.back();
  return out;
}
ActivityResolver createActivityDescriptionResolver(const gadgets::Gadgets& gadgets){
  return [gadgets](const std::string& name,const Json& input)->std::optional<std::string>{
    const auto* gadget=gadgets::findGadgetByName(gadgets,name);
    if(!gadget||!gadget->getActivityDescription)return std::nullopt;
    const Json description=gadget->getActivityDescription(input);
    return description.is_string()?
      std::optional<std::string>(description.get<std::string>()):std::nullopt;
  };
}
} // namespace batbox::bats
