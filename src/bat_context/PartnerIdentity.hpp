#pragma once
#include "bat_context/PartnerContext.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace batbox::bat_context {
struct DynamicTeamContext {
  std::string batId,batName,teamName;
  std::optional<std::string> color;
  bool planModeRequired=false;
  std::optional<std::string> parentFlightId;
};
struct PartnerIdentityState {
  mutable std::mutex mutex;
  std::shared_ptr<DynamicTeamContext> dynamicTeamContext;
};
PartnerIdentityState& sharedPartnerIdentityState();
void setDynamicTeamContext(PartnerIdentityState& state,
  std::shared_ptr<DynamicTeamContext> context);
void clearDynamicTeamContext(PartnerIdentityState& state);
std::shared_ptr<DynamicTeamContext> getDynamicTeamContext(
  const PartnerIdentityState& state);
std::optional<std::string> getParentFlightId(
  const PartnerIdentityState& state);
std::optional<std::string> getBatId(const PartnerIdentityState& state);
std::optional<std::string> getBatName(const PartnerIdentityState& state);
std::optional<std::string> getTeamName(const PartnerIdentityState& state,
  const std::optional<std::string>& appStateTeamName={});
bool isPartner(const PartnerIdentityState& state);
std::optional<std::string> getPartnerColor(
  const PartnerIdentityState& state);
bool isPlanModeRequired(const PartnerIdentityState& state,
  const std::function<bool()>& envPlanModeRequired);
bool isTeamLead(const PartnerIdentityState& state,
  const std::optional<std::string>& leadBatId);
std::function<bool()> indigenousPlanModeRequiredEnvironment();
}
