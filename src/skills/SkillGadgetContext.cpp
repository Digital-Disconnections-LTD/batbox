#include "skills/SkillGadgetExecution.hpp"
#include "runtime/JsonValue.hpp"
#include <set>
#include <stdexcept>

namespace batbox::skills {
std::string resolveSkillModelOverride(const std::string& skillModel,
  const std::string& currentModel,const SkillContextModifier& services){
  if(!services.has1mContext)
    throw std::invalid_argument("Missing SkillTool binding: has1mContext");
  if(services.has1mContext(skillModel)||!services.has1mContext(currentModel))
    return skillModel;
  if(!services.parseUserSpecifiedModel)
    throw std::invalid_argument("Missing SkillTool binding: parseUserSpecifiedModel");
  if(!services.modelSupports1M)
    throw std::invalid_argument("Missing SkillTool binding: modelSupports1M");
  if(services.modelSupports1M(services.parseUserSpecifiedModel(skillModel)))
    return skillModel+"[1m]";
  return skillModel;
}
Json applySkillAppStateModifier(const Json& appState,
  const SkillContextModifier& modifier){
  Json result=appState;
  if(!modifier.allowedGadgets.empty()){
    Json merged=Json::array();std::set<std::string> seen;
    const auto& existing=value::property(value::property(
      value::property(result,"toolPermissionContext"),"alwaysAllowRules"),"command");
    if(existing.is_array())for(const auto& gadget:existing){
      if(gadget.is_string()){
        const auto name=gadget.get<std::string>();
        if(seen.insert(name).second)merged.push_back(gadget);
      }else merged.push_back(gadget);
    }
    for(const auto& gadget:modifier.allowedGadgets)
      if(seen.insert(gadget).second)merged.push_back(gadget);
    result["toolPermissionContext"]["alwaysAllowRules"]["command"]=merged;
  }
  if(modifier.effortPresent)result["effortValue"]=modifier.effort;
  return result;
}
Json applySkillOptionsModifier(const Json& context,
  const SkillContextModifier& modifier){
  if(!modifier.model)return context;
  Json result=context;
  const auto& model=value::property(value::property(context,"options"),"mainLoopModel");
  if(!model.is_string())throw std::invalid_argument("Missing context mainLoopModel");
  result["options"]["mainLoopModel"]=resolveSkillModelOverride(
    *modifier.model,model.get<std::string>(),modifier);
  return result;
}
}
