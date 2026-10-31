#include "poetryEntries/EffectiveSystemPoetry.hpp"
#include <stdexcept>

namespace batbox::poetryEntries {
namespace {
template<class F>void need(const F& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Effective prompt import required: ")+name);
}
Json append(Json poetry,const Json& suffix){
  if(value::truthy(suffix))poetry.push_back(suffix);
  return poetry;
}
}
Json buildEffectiveSystemPoetry(const EffectivePoetryParams& p,
    const EffectivePoetryServices& s){
  if(value::truthy(p.overrideSystemPoetry))return Json::array({p.overrideSystemPoetry});
  need(s.feature,"feature");
  if(s.feature("COORDINATOR_MODE")){
    need(s.coordinatorModeEnabled,"coordinatorModeEnabled");
    if(s.coordinatorModeEnabled()&&!p.mainThreadBat){
      need(s.getCoordinatorSystemPoetry,"getCoordinatorSystemPrompt");
      return append(Json::array({s.getCoordinatorSystemPoetry()}),p.appendSystemPoetry);
    }
  }
  Json batPoetry(Json::value_t::discarded);
  if(p.mainThreadBat){
    need(s.isBuiltInBat,"isBuiltInAgent");
    need(p.mainThreadBat->getSystemPoetry,"agent.getSystemPrompt");
    if(s.isBuiltInBat(p.mainThreadBat->definition)){
      execution::Context optionsOnly;
      optionsOnly.data["options"]=value::property(p.gadgetUseContext.data,"options");
      optionsOnly.gadgets=p.gadgetUseContext.gadgets;
      optionsOnly.baseGadgets=p.gadgetUseContext.baseGadgets;
      batPoetry=p.mainThreadBat->getSystemPoetry(&optionsOnly);
    }else batPoetry=p.mainThreadBat->getSystemPoetry(nullptr);
  }
  // Bat memory does not emit telemetry here.
  if(value::truthy(batPoetry)&&(s.feature("PROACTIVE")||s.feature("KAIROS"))){
    if(s.isProactiveActive&&s.isProactiveActive()){
      Json poetry=p.defaultSystemPoetry;
      poetry.push_back("\n# Custom Agent Instructions\n"+batPoetry.get<std::string>());
      return append(std::move(poetry),p.appendSystemPoetry);
    }
  }
  return append(value::truthy(batPoetry)?Json::array({batPoetry}):
    value::truthy(p.customSystemPoetry)?Json::array({p.customSystemPoetry}):p.defaultSystemPoetry,
    p.appendSystemPoetry);
}
} // namespace batbox::poetryEntries
