#include "cli/permissions/PoetryHelpers.hpp"
#include <stdexcept>

namespace batbox::cli::permissions {
std::optional<std::string> serializeDecisionReason(
    const Json& reason,const std::function<bool(const std::string&)>& feature){
  if(!value::truthy(reason))return {};
  if(!feature)throw std::invalid_argument("Classifier feature import required");
  const auto& type=value::property(reason,"type");
  if((feature("BASH_CLASSIFIER")||feature("TRANSCRIPT_CLASSIFIER"))&&
     type=="classifier"){
    const auto& detail=value::property(reason,"reason");
    return detail.is_string()?std::optional<std::string>(
      detail.get<std::string>()):std::nullopt;
  }
  if(type=="hook"||type=="asyncAgent"||type=="sandboxOverride"||
     type=="workingDir"||type=="safetyCheck"||type=="other"){
    const auto& detail=value::property(reason,"reason");
    return detail.is_string()?std::optional<std::string>(
      detail.get<std::string>()):std::nullopt;
  }
  return {};
}
Json buildRequiresActionDetails(const gadgets::Gadget& gadget,const Json& input,
    const std::string& gadgetUseID,const std::string& requestId){
  Json description;
  try{
    if(gadget.getActivityDescription)
      description=gadget.getActivityDescription(input);
    if(description.is_null()&&gadget.getGadgetUseSummary)
      description=gadget.getGadgetUseSummary(input);
    if(description.is_null()){
      if(!gadget.userFacingName)
        throw std::invalid_argument("Tool user-facing name required");
      description=gadget.userFacingName(input);
    }
  }catch(...){description=gadget.name;}
  return Json{{"tool_name",gadget.name},{"action_description",description},
    {"tool_use_id",gadgetUseID},{"request_id",requestId},{"input",input}};
}
}
