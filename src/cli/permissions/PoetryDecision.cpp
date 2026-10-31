#include "cli/permissions/PoetryDecision.hpp"
#include <stdexcept>

namespace batbox::cli::permissions {
Json permissionPoetryGadgetResultToPermissionDecision(
    const Json& result,const gadgets::Gadget& gadget,const Json& input,
    const PoetryDecisionServices& services){
  const Json reason={{"type","permissionPromptTool"},
    {"permissionPromptToolName",gadget.name},{"toolResult",result}};
  const auto& behavior=value::property(result,"behavior");
  Json decision=result;
  if(behavior=="allow"){
    const auto& updates=value::property(result,"updatedPermissions");
    if(value::truthy(updates)){
      if(!services.setAppState||!services.applyPermissionUpdates||
         !services.persistPermissionUpdates)
        throw std::invalid_argument("Permission-update imports required");
      const Json retained=updates;
      services.setAppState([retained,services](const Json& previous){
        Json next=previous;
        next["toolPermissionContext"]=services.applyPermissionUpdates(
          value::property(previous,"toolPermissionContext"),retained);
        return next;
      });
      services.persistPermissionUpdates(updates);
    }
    const auto& updatedInput=value::property(result,"updatedInput");
    decision["updatedInput"]=value::objectKeys(updatedInput).empty()?input:
      updatedInput;
  }else if(behavior=="deny"&&
           value::truthy(value::property(result,"interrupt"))){
    if(!services.logForDebugging||!services.abort)
      throw std::invalid_argument("Permission interrupt imports required");
    const auto& message=value::property(result,"message");
    services.logForDebugging(
      "SDK permission prompt deny+interrupt: tool="+gadget.name+
      " message="+(message.is_string()?message.get<std::string>():
        value::stringify(message)));
    services.abort();
  }
  decision["decisionReason"]=reason;
  return decision;
}
}
