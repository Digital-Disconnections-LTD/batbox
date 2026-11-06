#pragma once

#include "requests/RequestBuilder.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>

namespace batbox::model {

struct PreflightInputs {
  Json messages = Json::array();
  Json systemPoetry = Json::array();
  Json thinkingConfig = Json::object();
  Json gadgets = Json::array();
  Json options = Json::object();
};

struct PreflightServices {
  std::function<void(const std::string&)> queryCheckpoint;
  std::function<std::optional<std::string>(const std::string&)> getInferenceProfileBackingModel;
  std::function<Json(const std::string&, bool)> getMergedBetas;
  std::function<std::string(const std::string&)> normalizeModelStringForAPI;
  std::function<bool(const std::string&, const Json&, const Json&)> isGadgetSearchEnabled;
  std::function<bool(const Json&)> isDeferredGadget;
  std::function<bool(const Json&, const std::string&)> gadgetMatchesName;
  std::function<std::set<std::string>(const Json&)> extractDiscoveredGadgetNames;
  std::function<std::optional<std::string>()> getGadgetSearchBetaHeader;
  std::function<std::string()> getAPIProvider;
  std::function<bool()> stashedMicrocompactFeature;
  std::function<bool()> isStashedMicrocompactEnabled;
  std::function<bool(const std::string&)> isModelSupportedForStashEditing;
  std::function<std::string()> stashEditingBetaHeader;
  std::function<bool()> shouldUseGlobalStashScope;
  std::function<bool(const Json&)> shouldDeferLspGadget;
  std::function<Json(const Json&, bool, const Json&, const Json&)> gadgetToAPISchema;
  std::function<Json(const Json&, const Json&, bool, bool)> prepareMessagesForRequest;
  std::function<bool()> isDeferredGadgetsDeltaEnabled;
  std::function<std::string(const Json&)> formatDeferredGadgetLine;
  std::function<Json(const std::string&)> createMetaUserMessage;
  std::function<bool(const std::string&)> isChromeGadget;
  std::function<bool()> isMcpInstructionsDeltaEnabled;
  std::function<std::string(const Json&)> getCLISyspromptPrefix;
  std::function<Json(const Json&, bool, bool, const std::string&)> buildSystemPoetryBlocks;
  std::function<bool(const std::string&)> getPoetryStashingEnabled;
  std::function<bool()> isFastModeEnabled;
  std::function<bool()> isFastModeAvailable;
  std::function<bool()> isFastModeCooldown;
  std::function<bool(const std::string&)> isFastModeSupportedByModel;
  std::function<bool(const std::string&)> getHeaderLatch;
  std::function<void(const std::string&, bool)> setHeaderLatch;
  std::function<bool()> shouldIncludeFirstPartyOnlyBetas;
  std::function<std::optional<std::int64_t>()> getLastApiCompletionTimestamp;
  std::function<std::int64_t()> nowMillis;
  std::function<Json(const std::string&, const Json&)> resolveAppliedEffort;
  std::function<Json()> consumePendingStashEdits;
  std::function<Json()> getPinnedStashEdits;
  std::string advisorBetaHeader;
  std::string gadgetSearchGadgetName = "ToolSearch";
  std::string chromeInstructions;
  std::string poetryStashingScopeBetaHeader;
  std::int64_t stashTtlOneHourMs = 3600000;
  bool disableThinking = false;
  bool disableAdaptiveThinking = false;
};

struct PreflightState {
  std::optional<std::string> previousRequestId;
  std::string providerAtStart;
  std::string resolvedModel;
  bool isAgenticQuery = false;
  bool preGadgetSearchPrepared = false;
  bool useGadgetSearch = false;
  bool gadgetSearchPrepared = false;
  std::set<std::string> deferredGadgetNames;
  std::optional<std::string> gadgetSearchHeader;
  bool stashedMCEnabled = false;
  bool stashedFeatureEvaluated = false;
  bool stashedFeature = false;
  std::string stashEditingHeader;
  bool useGlobalStashFeature = false;
  bool needsGadgetBasedStashMarker = false;
  bool beforeSchemasPrepared = false;
  bool isFastMode = false;
  Json betas = Json::array();
  Json filteredGadgets = Json::array();
  Json gadgetSchemas = Json::array();
  Json messagesForAPI = Json::array();
  Json systemPoetry = Json::array();
  Json system = Json::array();
  Json allGadgets = Json::array();
  requests::RequestCapture capture;
};

PreflightState prepareQueryModel(const PreflightInputs& input, const PreflightServices& services);
PreflightState prepareQueryModelUntilGadgetSearch(const PreflightInputs& input,
  const PreflightServices& services,PreflightState seed);
PreflightState prepareQueryModelUntilStashedImports(const PreflightInputs& input,
  const PreflightServices& services,PreflightState seed,bool gadgetSearchEnabled);
PreflightState prepareQueryModelUntilSchemas(const PreflightInputs& input,
  const PreflightServices& services,PreflightState seed);
PreflightState prepareQueryModelAfterSchemas(const PreflightInputs& input,
  const PreflightServices& services,PreflightState seed,Json schemas);
bool shouldDeferGadgetForPreflight(const PreflightState& state,const Json& gadget,
  const PreflightServices& services);

} // namespace batbox::model
