#include "bats/BatGadgets.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <unordered_map>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
template<class T>void need(const T& callback,const char* key){
  if(!callback)throw std::invalid_argument(std::string("Unbound BatGadget import: ")+key);
}
Json rule(const std::string& spec,BatGadgetFilterImports& imports){
  need(imports.permissionRuleValueFromString,"permissionRuleValueFromString");
  return imports.permissionRuleValueFromString(spec);
}
}
gadgets::Gadgets filterGadgetsForBat(const gadgets::Gadgets& available,bool isBuiltIn,
  bool isAsync,const std::optional<std::string>& permissionMode,
  BatGadgetFilterImports& imports){
  gadgets::Gadgets filtered;
  for(const auto* gadget:available){
    if(!gadget)continue;
    const auto& name=gadget->name;
    if(name.starts_with("mcp__")){filtered.push_back(gadget);continue;}
    if(permissionMode&&*permissionMode=="plan"){
      need(imports.gadgetMatchesName,"toolMatchesName");
      if(imports.gadgetMatchesName(*gadget,imports.exitPlanModeGadgetName)){
        filtered.push_back(gadget);continue;
      }
    }
    if(imports.allBatDisallowed.contains(name))continue;
    if(!isBuiltIn&&imports.customBatDisallowed.contains(name))continue;
    if(isAsync&&!imports.asyncBatAllowed.contains(name)){
      need(imports.isBatSwarmsEnabled,"isAgentSwarmsEnabled");
      if(imports.isBatSwarmsEnabled()){
        need(imports.isInProcessPartner,"isInProcessTeammate");
        if(imports.isInProcessPartner()){
          need(imports.gadgetMatchesName,"toolMatchesName");
          if(imports.gadgetMatchesName(*gadget,imports.batGadgetName)||
             imports.inProcessPartnerAllowed.contains(name)){
            filtered.push_back(gadget);continue;
          }
        }
      }
      continue;
    }
    filtered.push_back(gadget);
  }
  return filtered;
}
ResolvedBatGadgets resolveBatGadgets(const Json& batDefinition,
  const gadgets::Gadgets& available,bool isAsync,bool isMainThread,
  BatGadgetFilterImports& imports){
  ResolvedBatGadgets result;
  const Json& source=field(batDefinition,"source");
  const Json& mode=field(batDefinition,"permissionMode");
  const std::optional<std::string> permissionMode=mode.is_string()?
    std::optional<std::string>(mode.get<std::string>()):std::nullopt;
  gadgets::Gadgets filtered=isMainThread?available:filterGadgetsForBat(available,
    source=="built-in",isAsync,permissionMode,imports);
  std::unordered_set<std::string> disallowed;
  const Json& disallowedSpecs=field(batDefinition,"disallowedTools");
  if(disallowedSpecs.is_array())for(const Json& spec:disallowedSpecs){
    if(!spec.is_string())continue;
    const Json parsed=rule(spec.get<std::string>(),imports);
    const Json& name=field(parsed,"toolName");
    if(name.is_string())disallowed.insert(name.get<std::string>());
  }
  gadgets::Gadgets allowed;
  for(const auto* gadget:filtered)if(gadget&&!disallowed.contains(gadget->name))
    allowed.push_back(gadget);
  const Json& specs=field(batDefinition,"tools");
  const bool omitted=!batDefinition.is_object()||!batDefinition.contains("tools");
  result.hasWildcard=omitted||
    (specs.is_array()&&specs.size()==1&&specs[0]=="*");
  if(result.hasWildcard){result.resolvedGadgets=std::move(allowed);return result;}
  std::unordered_map<std::string,const gadgets::Gadget*> availableMap;
  for(const auto* gadget:allowed)if(gadget)availableMap[gadget->name]=gadget;
  std::unordered_set<const gadgets::Gadget*> resolvedSet;
  if(specs.is_array())for(const Json& gadgetSpec:specs){
    if(!gadgetSpec.is_string())continue;
    const std::string spec=gadgetSpec.get<std::string>();
    const Json parsed=rule(spec,imports);
    const Json& nameValue=field(parsed,"toolName");
    const std::string name=nameValue.is_string()?nameValue.get<std::string>():"";
    const Json& ruleContent=field(parsed,"ruleContent");
    if(name==imports.batGadgetName){
      if(value::truthy(ruleContent)&&ruleContent.is_string()){
        std::vector<std::string> types;
        const std::string content=ruleContent.get<std::string>();
        std::size_t begin=0;
        for(;;){
          const auto comma=content.find(',',begin);
          types.push_back(value::trim(std::string_view(content).substr(begin,
            comma==std::string::npos?std::string::npos:comma-begin)));
          if(comma==std::string::npos)break;
          begin=comma+1;
        }
        result.allowedBatTypes=std::move(types);
      }
      if(!isMainThread){result.validGadgets.push_back(spec);continue;}
    }
    const auto found=availableMap.find(name);
    if(found!=availableMap.end()){
      result.validGadgets.push_back(spec);
      if(resolvedSet.insert(found->second).second)
        result.resolvedGadgets.push_back(found->second);
    }else result.invalidGadgets.push_back(spec);
  }
  return result;
}
} // namespace batbox::bats
