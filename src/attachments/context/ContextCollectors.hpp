#pragma once
#include "attachments/Leaves.hpp"
#include "bats/BatGadgetPreflight.hpp"
#include "app_state/State.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::attachments::context {
struct ClientInstruction {std::string serverName,block;};
struct DeferralConfig {
  std::string searchGadgetName="ToolSearch",batGadgetName="Agent";
  std::optional<std::string> briefGadgetName,sendUserFileGadgetName;
  std::function<bool(const std::string&)> feature;
  std::function<bool()> forkSubagentEnabled,replBridgeActive;
};
bool isDeferredGadget(const gadgets::Gadget& gadget,const DeferralConfig& config);
bool isGadgetSearchGadgetAvailable(const gadgets::Gadgets& gadgets,
  const std::string& searchGadgetName);
std::optional<Json> getDeferredGadgetsDelta(const gadgets::Gadgets& gadgets,
  const Json& messages,const DeferralConfig& config);
std::optional<Json> getMcpInstructionsDelta(const Json& clients,
  const Json& messages,const std::vector<ClientInstruction>& clientSide);
std::string getGadgetsDescription(const Json& bat);
std::string formatBatLine(const Json& bat);

struct Services {
  attachments::LeafServices leaves;
  DeferralConfig deferral;
  std::function<bool()> deferredGadgetsDeltaEnabled,
    gadgetSearchEnabledOptimistic,batListInMessages,
    mcpInstructionsDeltaEnabled,tokenUsageAttachmentEnabled;
  std::function<bool(const std::string&)> modelSupportsGadgetReference;
  std::function<std::optional<ClientInstruction>()> clientSideInstruction;
  std::function<app_state::AppStateRef()> getAppState;
  std::function<Json(const Json&,const Json&,const std::string&)>
    filterDeniedBats;
  std::function<std::string()> subscriptionType;
  std::function<double(const std::string&)> effectiveContextWindow;
  std::string batGadgetName="Agent",searchGadgetName="ToolSearch";
};
std::optional<Json> getBatListingDelta(const gadgets::Gadgets& gadgets,
  const Json& batDefinitions,const Json& messages,const Services& services);
void bindContextCollectors(attachments::Services& aggregation,
  Services services);
} // namespace batbox::attachments::context
