#include "app_state/Selectors.hpp"

namespace batbox::app_state {
namespace {
std::optional<std::string> viewedId(const AppState& state){
  const auto value=state.fieldRef("viewingAgentTaskId");
  if(!value||!value::truthy(*value)||!value->is_string())return {};
  return value->get<std::string>();
}
}
value::ValueRef getViewedPartnerMission(const AppState& state){
  const auto id=viewedId(state);
  if(!id||!state.missions)return {};
  const auto mission=state.missions->get(*id);
  if(!mission||!*mission||!value::truthy(**mission))return {};
  const auto& value=**mission;
  if(!value.is_object()||value::property(value,"type")!="in_process_teammate")
    return {};
  return *mission;
}
Json ActiveBatForInput::snapshot()const{
  if(type==Type::Leader)return {{"type","leader"}};
  return {{"type",type==Type::Viewed?"viewed":"named_agent"},
    {"task",mission?*mission:Json(nullptr)}};
}
ActiveBatForInput getActiveBatForInput(const AppState& state){
  if(auto viewed=getViewedPartnerMission(state))
    return {ActiveBatForInput::Type::Viewed,std::move(viewed)};
  const auto id=viewedId(state);
  if(id&&state.missions){
    const auto mission=state.missions->get(*id);
    if(mission&&*mission&&value::property(**mission,"type")=="local_agent")
      return {ActiveBatForInput::Type::NamedBat,*mission};
  }
  return {};
}
} // namespace batbox::app_state
