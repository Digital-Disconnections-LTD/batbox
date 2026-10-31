#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace batbox::betas {
struct Headers {
  std::string core,internal,oauth,context1m,interleavedThinking,
    redactThinking,summarizeConnectorText,contextManagement,
    structuredOutputs,tokenEfficientGadgets,webSearch,
    poetryStashingScope,gadgetSearchFirstParty,gadgetSearchThirdParty;
  std::vector<std::string> bedrockExtraBodyParams;
};
struct ModelPatterns {
  std::string legacy,opus4,sonnet4,haiku4,haiku,
    autoDenyRegex,autoAllowRegex;
  std::vector<std::string> structuredOutputs;
};
struct EnvironmentKeys {
  std::string userType="USER_TYPE",entrypoint="BATBOX_ENTRYPOINT",
    disableExperimental="BATBOX_DISABLE_EXPERIMENTAL_BETAS",
    disableInterleaved="DISABLE_INTERLEAVED_THINKING",
    summarizeConnector="USE_CONNECTOR_TEXT_SUMMARIZATION",
    contextManagement="USE_API_CONTEXT_MANAGEMENT",
    additionalBetas="BATBOX_BETAS";
  std::string internalUserType="batbox";
};
struct Services {
  std::function<Json(const std::string&)> environment;
  std::function<std::string(const std::string&)> canonicalName;
  std::function<std::string()> provider;
  std::function<std::optional<bool>(const std::string&,
    const std::string&)> capabilityOverride;
  std::function<bool()> isSubscriber,nonInteractive,
    showThinkingSummaries;
  std::function<bool(const std::string&)> has1mContext,featureGate,
    buildFeature;
  std::function<Json(const std::string&,const Json&)> featureValue;
  std::function<std::optional<std::vector<std::string>>()> sdkBetas;
  std::function<void(const std::string&)> warn;
  Headers headers;
  ModelPatterns patterns;
  EnvironmentKeys environmentKeys;
};
class BetaHeaders {
public:
  explicit BetaHeaders(Services services);
  std::optional<std::vector<std::string>> filterAllowedSdkBetas(
    const std::optional<std::vector<std::string>>& sdkBetas);
  bool modelSupportsISP(const std::string& model)const;
  bool vertexModelSupportsWebSearch(const std::string& model)const;
  bool modelSupportsContextManagement(const std::string& model)const;
  bool modelSupportsStructuredOutputs(const std::string& model)const;
  bool modelSupportsAutoMode(const std::string& model)const;
  std::string getGadgetSearchBetaHeader()const;
  bool shouldIncludeFirstPartyOnlyBetas()const;
  bool shouldUseGlobalStashScope()const;
  std::shared_ptr<std::vector<std::string>> getAllModelBetas(
    const std::string& model);
  std::shared_ptr<std::vector<std::string>> getModelBetas(
    const std::string& model);
  std::shared_ptr<std::vector<std::string>> getBedrockExtraBodyParamsBetas(
    const std::string& model);
  std::vector<std::string> getMergedBetas(const std::string& model,
    bool isAgenticQuery=false);
  void clearBetasStashes();
private:
  Services services_;
  std::unordered_map<std::string,
    std::shared_ptr<std::vector<std::string>>> all_,model_,bedrockExtra_;
  Json env(const std::string& key)const;
  bool internalCli()const;
};
}
