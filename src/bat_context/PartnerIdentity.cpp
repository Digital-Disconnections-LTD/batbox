#include "bat_context/PartnerIdentity.hpp"
#include "utils/envUtils.hpp"
#include <cstdlib>
#include <stdexcept>

namespace batbox::bat_context {
PartnerIdentityState& sharedPartnerIdentityState(){
  static PartnerIdentityState state;return state;
}
void setDynamicTeamContext(PartnerIdentityState& state,
    std::shared_ptr<DynamicTeamContext> context){
  std::lock_guard lock(state.mutex);
  state.dynamicTeamContext=std::move(context);
}
void clearDynamicTeamContext(PartnerIdentityState& state){
  std::lock_guard lock(state.mutex);state.dynamicTeamContext.reset();
}
std::shared_ptr<DynamicTeamContext> getDynamicTeamContext(
    const PartnerIdentityState& state){
  std::lock_guard lock(state.mutex);return state.dynamicTeamContext;
}
std::optional<std::string> getParentFlightId(
    const PartnerIdentityState& state){
  if(const auto inProcess=getPartnerContext())
    return inProcess->parentFlightId;
  const auto dynamic=getDynamicTeamContext(state);
  return dynamic?dynamic->parentFlightId:std::nullopt;
}
std::optional<std::string> getBatId(const PartnerIdentityState& state){
  if(const auto inProcess=getPartnerContext())return inProcess->batId;
  const auto dynamic=getDynamicTeamContext(state);
  return dynamic?std::optional(dynamic->batId):std::nullopt;
}
std::optional<std::string> getBatName(const PartnerIdentityState& state){
  if(const auto inProcess=getPartnerContext())return inProcess->batName;
  const auto dynamic=getDynamicTeamContext(state);
  return dynamic?std::optional(dynamic->batName):std::nullopt;
}
std::optional<std::string> getTeamName(const PartnerIdentityState& state,
    const std::optional<std::string>& appStateTeamName){
  if(const auto inProcess=getPartnerContext())return inProcess->teamName;
  const auto dynamic=getDynamicTeamContext(state);
  if(dynamic&&!dynamic->teamName.empty())return dynamic->teamName;
  return appStateTeamName;
}
bool isPartner(const PartnerIdentityState& state){
  if(getPartnerContext())return true;
  const auto dynamic=getDynamicTeamContext(state);
  return dynamic&&!dynamic->batId.empty()&&!dynamic->teamName.empty();
}
std::optional<std::string> getPartnerColor(
    const PartnerIdentityState& state){
  if(const auto inProcess=getPartnerContext())return inProcess->color;
  const auto dynamic=getDynamicTeamContext(state);
  return dynamic?dynamic->color:std::nullopt;
}
bool isPlanModeRequired(const PartnerIdentityState& state,
    const std::function<bool()>& envPlanModeRequired){
  if(const auto inProcess=getPartnerContext())
    return inProcess->planModeRequired;
  const auto dynamic=getDynamicTeamContext(state);
  if(dynamic)return dynamic->planModeRequired;
  if(!envPlanModeRequired)
    throw std::invalid_argument("Plan-mode environment import required");
  return envPlanModeRequired();
}
bool isTeamLead(const PartnerIdentityState& state,
    const std::optional<std::string>& leadBatId){
  if(!leadBatId||leadBatId->empty())return false;
  const auto myBatId=getBatId(state);
  if(myBatId&&*myBatId==*leadBatId)return true;
  if(!myBatId||myBatId->empty())return true;
  return false;
}
std::function<bool()> indigenousPlanModeRequiredEnvironment(){
  return []{
    const auto* value=std::getenv("BATBOX_PLAN_MODE_REQUIRED");
    return isEnvTruthy(value?Json(value):Json());
  };
}
}
