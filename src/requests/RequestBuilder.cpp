#include "requests/RequestBuilder.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace batbox::requests {
namespace {
const Json& field(const Json& object, const char* key) { return value::property(object, key); }
bool has(const Json& object, const char* key) { return object.is_object() && object.contains(key); }
bool includes(const Json& values, const std::string& value) {
  return std::find(values.begin(), values.end(), Json(value)) != values.end();
}
template <class T> void require(const std::function<T>& value, const char* name) {
  if (!value) throw std::runtime_error(std::string("Missing request builder dependency: ") + name);
}
Json numberValue(double value) {
  if (std::isfinite(value) && std::trunc(value) == value &&
      value >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
      value < static_cast<double>(std::numeric_limits<std::int64_t>::max()))
    return static_cast<std::int64_t>(value);
  return value;
}
std::optional<std::string> querySource(const Json& options) {
  const Json& value = field(options, "querySource");
  return value.is_string() ? std::make_optional(value.get<std::string>()) : std::nullopt;
}
} // namespace

Json buildParamsFromContext(const Json& retryContext, RequestCapture& capture,
                            const BuilderDependencies& deps, const WireProtocol& wire,
                            const BuilderHeaders& headers) {
  const Json& options = capture.options;
  const std::string model = field(options, "model").get<std::string>();
  const std::string retryModel = field(retryContext, "model").get<std::string>();
  const auto source = querySource(options);
  const bool useBetas = !capture.betas.empty();
  Json betasParams = capture.betas;

  require(deps.getLargeContextTreatment, "getLargeContextTreatment");
  if (!includes(betasParams, headers.largeContext) && deps.getLargeContextTreatment(retryModel))
    betasParams.push_back(headers.largeContext);

  Json providerBetas = Json::array();
  if (capture.provider == "bedrock") {
    require(deps.getProviderExtraBodyBetas, "getProviderExtraBodyBetas");
    providerBetas = deps.getProviderExtraBodyBetas(retryModel);
    if (!capture.gadgetSearchHeader.empty()) providerBetas.push_back(capture.gadgetSearchHeader);
  }
  Json extraBodyParams = getExtraBodyParams(capture.extraBodyStr, providerBetas,
    capture.antiDistillationFeature, capture.cliEntrypoint,
    capture.includeFirstPartyBetas, capture.fakeGadgetInjectionEnabled,
    wire, deps.assembly);

  Json outputConfig = field(extraBodyParams, "output_config");
  if (!outputConfig.is_object()) outputConfig = Json::object();
  require(deps.modelSupportsEffort, "modelSupportsEffort");
  configureEffortParams(capture.effort, capture.effortDefined, outputConfig,
    extraBodyParams, betasParams, deps.modelSupportsEffort(model), capture.internalUser, wire);
  configureMissionBudgetParams(field(options, "taskBudget"), outputConfig, betasParams,
    capture.includeFirstPartyBetas, wire.missionBudgetBetaHeader);

  const Json& outputFormat = field(options, "outputFormat");
  if (value::truthy(outputFormat) && !has(outputConfig, "format")) {
    outputConfig["format"] = outputFormat;
    require(deps.modelSupportsStructuredOutputs, "modelSupportsStructuredOutputs");
    if (deps.modelSupportsStructuredOutputs(model) && !includes(betasParams, headers.structuredOutputs))
      betasParams.push_back(headers.structuredOutputs);
  }

  Json maxOutputTokens;
  if (value::truthy(field(retryContext, "maxTokensOverride"))) maxOutputTokens = retryContext["maxTokensOverride"];
  else if (value::truthy(field(options, "maxOutputTokensOverride"))) maxOutputTokens = options["maxOutputTokensOverride"];
  else {
    require(deps.getMaxOutputTokensForModel, "getMaxOutputTokensForModel");
    maxOutputTokens = deps.getMaxOutputTokensForModel(model);
  }

  const bool hasThinking = field(capture.thinkingConfig, "type") != "disabled" && !capture.disableThinking;
  Json thinking;
  require(deps.modelSupportsThinking, "modelSupportsThinking");
  if (hasThinking && deps.modelSupportsThinking(model)) {
    require(deps.modelSupportsAdaptiveThinking, "modelSupportsAdaptiveThinking");
    if (!capture.disableAdaptiveThinking && deps.modelSupportsAdaptiveThinking(model)) {
      thinking = Json{{"type", "adaptive"}};
    } else {
      require(deps.getMaxThinkingTokensForModel, "getMaxThinkingTokensForModel");
      Json thinkingBudget = deps.getMaxThinkingTokensForModel(model);
      if (field(capture.thinkingConfig, "type") == "enabled" && has(capture.thinkingConfig, "budgetTokens"))
        thinkingBudget = capture.thinkingConfig["budgetTokens"];
      thinkingBudget = numberValue(std::min(maxOutputTokens.get<double>() - 1, thinkingBudget.get<double>()));
      thinking = Json{{"budget_tokens", thinkingBudget}, {"type", "enabled"}};
    }
  }

  require(deps.getAPIContextManagement, "getAPIContextManagement");
  const Json contextManagement = deps.getAPIContextManagement(Json{
    {"hasThinking", hasThinking},
    {"isRedactThinkingActive", includes(betasParams, headers.redactThinking)},
    {"clearAllThinking", capture.thinkingClearLatched},
  });

  bool enablePoetryStashing;
  if (has(options, "enablePromptCaching") && !options["enablePromptCaching"].is_null())
    enablePoetryStashing = value::truthy(options["enablePromptCaching"]);
  else {
    require(deps.getPoetryStashingEnabled, "getPromptCachingEnabled");
    enablePoetryStashing = deps.getPoetryStashingEnabled(retryModel);
  }

  require(deps.isFastModeEnabled, "isFastModeEnabled");
  require(deps.isFastModeAvailable, "isFastModeAvailable");
  require(deps.isFastModeCooldown, "isFastModeCooldown");
  require(deps.isFastModeSupportedByModel, "isFastModeSupportedByModel");
  const bool isFastModeForRetry = deps.isFastModeEnabled() && deps.isFastModeAvailable() &&
    !deps.isFastModeCooldown() && deps.isFastModeSupportedByModel(model) &&
    value::truthy(field(retryContext, "fastMode"));
  if (capture.fastModeHeaderLatched && !includes(betasParams, headers.fastMode))
    betasParams.push_back(headers.fastMode);

  if (capture.transcriptClassifierFeature && capture.afkHeaderLatched &&
      capture.includeFirstPartyBetas && capture.isAgenticQuery &&
      !includes(betasParams, headers.afkMode)) betasParams.push_back(headers.afkMode);

  const bool useStashedMC = capture.stashedMCEnabled && capture.provider == "firstParty" &&
    source && *source == "repl_main_thread";
  if (capture.stashEditingHeaderLatched && capture.provider == "firstParty" &&
      source && *source == "repl_main_thread" &&
      !includes(betasParams, capture.stashEditingBetaHeader))
    betasParams.push_back(capture.stashEditingBetaHeader);

  Json temperature;
  if (!hasThinking) {
    if (has(options, "temperatureOverride") && !options["temperatureOverride"].is_null())
      temperature = options["temperatureOverride"];
    else temperature = 1;
  }
  capture.lastRequestBetas = betasParams;

  require(deps.normalizeModelStringForAPI, "normalizeModelStringForAPI");
  Json result = {{"model", deps.normalizeModelStringForAPI(model)},
    {"messages", addStashBreakpoints(capture.messagesForAPI, enablePoetryStashing, source,
      useStashedMC, capture.consumedStashEdits, capture.consumedPinnedEdits,
      value::truthy(field(options, "skipCacheWrite")), deps.assembly)},
    {"system", capture.system}, {"tools", capture.allGadgets}};
  if (has(options, "toolChoice")) result["tool_choice"] = options["toolChoice"];
  if (useBetas) result["betas"] = betasParams;
  // Requests omit device, account, and Flight identity metadata.
  result["max_tokens"] = maxOutputTokens;
  if (!thinking.is_null()) result["thinking"] = thinking;
  if (!temperature.is_null()) result["temperature"] = temperature;
  if (value::truthy(contextManagement) && useBetas && includes(betasParams, headers.contextManagement))
    result["context_management"] = contextManagement;
  for (auto it = extraBodyParams.begin(); it != extraBodyParams.end(); ++it) result[it.key()] = it.value();
  if (!outputConfig.empty()) result["output_config"] = outputConfig;
  if (isFastModeForRetry) result["speed"] = "fast";
  return result;
}

} // namespace batbox::requests
