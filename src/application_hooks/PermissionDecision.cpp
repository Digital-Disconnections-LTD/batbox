#include "application_hooks/Provider.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::application_hooks {
namespace {
std::string behavior(const Json& value){
  const auto& found=value::property(value,"behavior");
  return found.is_string()?found.get<std::string>():std::string{};
}
}
Json resolveHookPermissionDecision(const Json& hookPermissionResult,
  const gadgets::Gadget& gadget,const Json& input,const execution::Context& context,
  const Json& assistantMessage,const std::string& gadgetUseID,
  const PermissionImports& imports){
  if(!imports.canUseGadget||!imports.checkRuleBasedPermissions)
    throw std::invalid_argument("Hook permission callbacks required");
  const auto kind=behavior(hookPermissionResult);
  const bool requiresInteraction=gadget.requiresUserInteraction&&
    gadget.requiresUserInteraction();
  const bool requireCanUseGadget=value::truthy(value::property(context.data,
    "requireCanUseTool"));
  if(kind=="allow"){
    const bool updated=hookPermissionResult.is_object()&&
      hookPermissionResult.contains("updatedInput");
    const Json hookInput=updated?hookPermissionResult.at("updatedInput"):input;
    const bool interactionSatisfied=requiresInteraction&&updated;
    if((requiresInteraction&&!interactionSatisfied)||requireCanUseGadget)
      return {{"decision",imports.canUseGadget(gadget,hookInput,context,
        assistantMessage,gadgetUseID,std::nullopt).get()},{"input",hookInput}};
    const auto ruleCheck=imports.checkRuleBasedPermissions(gadget,hookInput,
      context).get();
    if(!ruleCheck)return {{"decision",hookPermissionResult},
      {"input",hookInput}};
    if(behavior(*ruleCheck)=="deny")
      return {{"decision",*ruleCheck},{"input",hookInput}};
    return {{"decision",imports.canUseGadget(gadget,hookInput,context,
      assistantMessage,gadgetUseID,std::nullopt).get()},{"input",hookInput}};
  }
  if(kind=="deny")return {{"decision",hookPermissionResult},
    {"input",input}};
  const auto forceDecision=kind=="ask"?
    std::optional<Json>(hookPermissionResult):std::nullopt;
  const auto& updated=value::property(hookPermissionResult,"updatedInput");
  const Json askInput=kind=="ask"&&value::truthy(updated)?updated:input;
  return {{"decision",imports.canUseGadget(gadget,askInput,context,
    assistantMessage,gadgetUseID,forceDecision).get()},{"input",askInput}};
}
} // namespace batbox::application_hooks
