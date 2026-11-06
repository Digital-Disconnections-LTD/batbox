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
// Kept free between prompt and max_tokens, since the prompt is estimated.
constexpr double CONTEXT_MARGIN_TOKENS = 1024;
// max_tokens is never fitted below this; a prompt that leaves less room is
// for the server to refuse and compaction to shrink.
constexpr double MIN_FITTED_OUTPUT_TOKENS = 3000;
// About four bytes of text per token; an image or document counts as 2000
// tokens whatever the size of its base64 data.
double roughTokens(const Json& value) {
  if (value.is_string()) return static_cast<double>(value.get_ref<const std::string&>().size()) / 4;
  double total = 0;
  if (value.is_array()) for (const Json& item : value) total += roughTokens(item);
  else if (value.is_object()) {
    if (field(value, "type") == "base64") return 2000;
    for (auto it = value.begin(); it != value.end(); ++it)
      total += static_cast<double>(it.key().size()) / 4 + roughTokens(it.value());
  } else total = 1;
  return total;
}
double promptTokens(const RequestCapture& capture) {
  return std::max(capture.knownPromptTokens, roughTokens(capture.messagesForAPI) +
    roughTokens(capture.system) + roughTokens(capture.allGadgets));
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
  if (!includes(betasParams, headers.largeContext) && deps.getLargeContextTreatment(retryModel) &&
      !headers.largeContext.empty())
    betasParams.push_back(headers.largeContext);

  Json providerBetas = Json::array();
  if (capture.provider == "bedrock") {
    require(deps.getProviderExtraBodyBetas, "getProviderExtraBodyBetas");
    providerBetas = deps.getProviderExtraBodyBetas(retryModel);
    if (!capture.gadgetSearchHeader.empty()) providerBetas.push_back(capture.gadgetSearchHeader);
  }
  Json extraBodyParams = getExtraBodyParams(capture.extraBodyStr, providerBetas,
    wire, deps.assembly);

  Json outputConfig = field(extraBodyParams, "output_config");
  if (!outputConfig.is_object()) outputConfig = Json::object();
  require(deps.modelSupportsEffort, "modelSupportsEffort");
  const bool supportsEffort=deps.modelSupportsEffort(model);
  if(capture.provider=="local"){
    // Local Messages servers accept effort directly, without a provider beta header.
    if(supportsEffort&&capture.effortDefined&&capture.effort.is_string()&&
       !has(outputConfig,"effort"))outputConfig["effort"]=capture.effort;
  }else configureEffortParams(capture.effort, capture.effortDefined, outputConfig,
    betasParams, supportsEffort, wire);
  configureMissionBudgetParams(field(options, "taskBudget"), outputConfig, betasParams,
    capture.includeFirstPartyBetas, wire.missionBudgetBetaHeader);

  const Json& outputFormat = field(options, "outputFormat");
  if (value::truthy(outputFormat) && !has(outputConfig, "format")) {
    outputConfig["format"] = outputFormat;
    require(deps.modelSupportsStructuredOutputs, "modelSupportsStructuredOutputs");
    if (deps.modelSupportsStructuredOutputs(model) && !headers.structuredOutputs.empty() &&
        !includes(betasParams, headers.structuredOutputs))
      betasParams.push_back(headers.structuredOutputs);
  }

  Json maxOutputTokens;
  if (value::truthy(field(retryContext, "maxTokensOverride"))) maxOutputTokens = retryContext["maxTokensOverride"];
  else if (value::truthy(field(options, "maxOutputTokensOverride"))) maxOutputTokens = options["maxOutputTokensOverride"];
  else {
    require(deps.getMaxOutputTokensForModel, "getMaxOutputTokensForModel");
    maxOutputTokens = deps.getMaxOutputTokensForModel(model);
  }
  // Prompt plus max_tokens must fit the context window, or servers such as
  // vLLM refuse the request. Only the value sent changes, not the setting.
  capture.outputCapFromContext = false;
  if (deps.getContextWindowForModel && maxOutputTokens.is_number()) {
    const double window = deps.getContextWindowForModel(model);
    const double requested = maxOutputTokens.get<double>();
    if (window > 0) {
      const double fitted = std::max(std::floor(window - promptTokens(capture) - CONTEXT_MARGIN_TOKENS),
        std::min(requested, MIN_FITTED_OUTPUT_TOKENS));
      if (fitted < requested) {
        maxOutputTokens = numberValue(fitted);
        capture.outputCapFromContext = true;
      }
    }
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
  if (capture.fastModeHeaderLatched && !headers.fastMode.empty() &&
      !includes(betasParams, headers.fastMode))
    betasParams.push_back(headers.fastMode);

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
