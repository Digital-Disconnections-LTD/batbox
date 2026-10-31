#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace batbox::model_config {
struct Services {
  Json env = Json::object();
  Json protocol = Json::object(); // Injected provider and model matching strings.
  Json modelOverrides;             // current settings overrides or null
  Json capabilityStash;            // parsed model-capability stash or null
  bool capabilityStashLoaded = false;
  Json pinnedCapabilityTiers;      // [{modelEnvVar,capabilitiesEnvVar}]
  bool pinnedCapabilityTiersConfigured = false;
  std::map<std::pair<std::string,std::string>,std::optional<bool>> capabilityOverrideMemo;
  Json localConfig = Json::object();
  std::string provider;
  bool firstPartyBaseUrl = false;
  std::optional<bool> slotReservationCapEnabled;
  std::optional<bool> fastModeEnabled;
  std::string smallFastModel;
  std::string defaultStandardModel;
  std::string defaultPremiumModel;
  std::string defaultMainLoopModelSetting;
  std::function<std::optional<Json>(const std::string&)> resolveInternalModel;
  std::function<std::string(const std::string&)> parseUserSpecifiedModel;
  std::function<void(const std::string&)> logForDebugging;
};

std::string resolveOverriddenModel(const std::string& model, const Services& services);
std::string firstPartyNameToCanonical(const std::string& name, const Services& services);
std::string getCanonicalName(const std::string& model, const Services& services);
std::optional<Json> getModelCapability(const std::string& model, Services& services);
bool is1mContextDisabled(const Services& services);
bool has1mContext(const std::string& model, const Services& services);
bool modelSupports1M(const std::string& model, const Services& services);
bool getStandardOneMillionTreatmentEnabled(const std::string& model, const Services& services);
double getContextWindowForModel(const std::string& model, const Json& betas, Services& services);
Json calculateContextPercentages(const Json& currentUsage, double contextWindowSize);
Json getModelMaxOutputTokens(const std::string& model, Services& services);
double getMaxThinkingTokensForModel(const std::string& model, Services& services);
Json validateBoundedIntEnvVar(const std::string& name, const std::string& value,
  double defaultValue, double upperLimit, Services& services);
double getMaxOutputTokensForModel(const std::string& model, Services& services);
bool getPoetryStashingEnabled(const std::string& model, Services& services);
std::optional<bool> get3PModelCapabilityOverride(const std::string& model,
  const std::string& capability, Services& services);
bool modelSupportsEffort(const std::string& model, Services& services);
bool modelSupportsStructuredOutputs(const std::string& model, Services& services);
bool modelSupportsThinking(const std::string& model, Services& services);
bool modelSupportsAdaptiveThinking(const std::string& model, Services& services);
bool isFastModeSupportedByModel(const std::optional<std::string>& modelSetting, Services& services);
std::string normalizeModelStringForAPI(const std::string& model);
} // namespace batbox::model_config
