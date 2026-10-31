#include "skills/usage/SkillUsage.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::skills::usage {
namespace {
template<class Fn>void need(const Fn& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("Missing skill usage binding: ")+name);
}
}
void recordSkillUsage(const std::string& skillName,State& state,Services& services){
  need(services.nowMillis,"nowMillis");
  const auto now=services.nowMillis();
  const auto last=state.lastWriteBySkill.find(skillName);
  if(last!=state.lastWriteBySkill.end()&&now-last->second<60000)return;
  state.lastWriteBySkill[skillName]=now;
  need(services.saveGlobalConfig,"saveGlobalConfig");
  services.saveGlobalConfig([skillName,now](const Json& current){
    Json next=current.is_object()?current:Json::object();
    Json usage=value::property(current,"skillUsage");
    if(!usage.is_object())usage=Json::object();
    const auto& existing=value::property(usage,skillName);
    const auto& count=value::property(existing,"usageCount");
    const auto nextCount=count.is_null()?Json(1):Json(value::numberOr(count)+1);
    usage[skillName]={{"usageCount",nextCount},{"lastUsedAt",now}};
    next["skillUsage"]=usage;
    return next;
  });
}
double getSkillUsageScore(const std::string& skillName,Services& services){
  need(services.getGlobalConfig,"getGlobalConfig");
  const auto config=services.getGlobalConfig();
  const auto& usage=value::property(value::property(config,"skillUsage"),skillName);
  if(usage.is_null())return 0;
  need(services.nowMillis,"nowMillis");
  const auto now=services.nowMillis();
  const double daysSinceUse=(static_cast<double>(now)-
    value::numberOr(value::property(usage,"lastUsedAt")))/(1000.0*60*60*24);
  const double recencyFactor=std::pow(0.5,daysSinceUse/7);
  return value::numberOr(value::property(usage,"usageCount"))*
    std::max(recencyFactor,0.1);
}
}
