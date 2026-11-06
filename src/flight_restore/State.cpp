#include "flight_restore/FlightRestore.hpp"
#include <stdexcept>

namespace batbox::flight_restore {
namespace {
void require(bool ready,const char* name){if(!ready)throw std::invalid_argument(std::string("Missing session restore import: ")+name);}
}

Json parseTodoList(const Json& value){
  if(!value.is_array())return Json::array();
  Json result=Json::array();
  for(const auto& item:value){
    const auto& content=value::property(item,"content");
    const auto& status=value::property(item,"status");
    const auto& active=value::property(item,"activeForm");
    if(!item.is_object()||!content.is_string()||content.get_ref<const std::string&>().empty()||
        !active.is_string()||active.get_ref<const std::string&>().empty()||
        (status!="pending"&&status!="in_progress"&&status!="completed"))
      return Json::array();
    result.push_back({{"content",content},{"status",status},{"activeForm",active}});
  }
  return result;
}

Json extractTodosFromTranscript(const Json& messages,Services& services){
  require(static_cast<bool>(services.parseTodoList),"parseTodoList");
  if(!messages.is_array())return Json::array();
  for(auto it=messages.rbegin();it!=messages.rend();++it){
    const Json& message=*it;
    if(value::property(message,"type")!="assistant")continue;
    const Json& blocks=value::property(value::property(message,"message"),"content");
    if(!blocks.is_array())throw std::runtime_error("assistant message content is not an array");
    const Json* gadgetUse=nullptr;
    for(const Json& block:blocks){
      if(value::property(block,"type")=="tool_use"&&
          value::property(block,"name")=="TodoWrite"){
        gadgetUse=&block;break;
      }
    }
    if(!gadgetUse)continue;
    const Json& input=value::property(*gadgetUse,"input");
    if(input.is_null()||(!input.is_object()&&!input.is_array()))return Json::array();
    return services.parseTodoList(value::property(input,"todos"));
  }
  return Json::array();
}

std::optional<Json> computeStandaloneBatContext(
  const std::optional<std::string>& batName,
  const std::optional<std::string>& batColor){
  if((!batName||batName->empty())&&(!batColor||batColor->empty()))
    return std::nullopt;
  Json out={{"name",batName.value_or("")}};
  if(batColor&&*batColor!="default")out["color"]=*batColor;
  return out;
}
} // namespace batbox::flight_restore
