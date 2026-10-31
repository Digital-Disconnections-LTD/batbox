#include "bootstrap/BootstrapBindings.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::bootstrap {
BootstrapBindings bindBootstrapState(std::shared_ptr<BootstrapState> state,
  std::function<double()> nowMs,bool developerUser){
  if(!state||!nowMs)
    throw std::invalid_argument("Bootstrap state and clock are required");
  BootstrapBindings bindings;
  bindings.setStashedBatboxMdContent=[state](
    const std::optional<std::string>& content){
    setStashedBatboxMdContent(state->request,content);
  };
  bindings.addInvokedSkill=[state,nowMs](const std::string& name,
    const std::string& path,const std::string& content,
    const std::optional<std::string>& bat){
    addInvokedSkill(state->invokedSkills,name,path,content,bat,nowMs);
  };
  bindings.clearInvokedSkillsForBat=[state](const std::string& bat){
    clearInvokedSkillsForBat(state->invokedSkills,bat);
  };
  bindings.getInvokedSkillsForBat=[state](const Json& bat){
    std::optional<std::string> batId;
    if(!bat.is_null())batId=bat.get<std::string>();
    const auto filtered=getInvokedSkillsForBat(state->invokedSkills,
      batId);
    Json result=Json::array();
    for(const auto& [key,skill]:filtered){
      (void)key;
      result.push_back({{"skillName",skill->skillName},
        {"skillPath",skill->skillPath},{"content",skill->content},
        {"invokedAt",skill->invokedAt},
        {"agentId",skill->batId?Json(*skill->batId):Json(nullptr)}});
    }
    return result;
  };
  bindings.addSlowOperation=[state,nowMs,developerUser](
    const std::string& operation,double duration){
    addSlowOperation(state->slow,operation,duration,developerUser,nowMs);
  };
  bindings.getRegisteredHooksForEvent=[state](const std::string& event)
    ->std::optional<std::vector<
      instructions::hooks::runtime::HookMatcher>>{
    const auto roster=getRegisteredHooks(state->hooks);
    if(!roster)return std::nullopt;
    for(const auto& [key,matchers]:*roster)if(key==event){
      std::vector<instructions::hooks::runtime::HookMatcher> result;
      result.reserve(matchers.size());
      for(const auto& matcher:matchers)result.push_back(*matcher);
      return result;
    }
    return std::nullopt;
  };
  return bindings;
}
}
