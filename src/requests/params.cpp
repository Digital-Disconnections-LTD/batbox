#include "requests/assembly.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace batbox::requests {
namespace {
bool has(const Json& object, const char* key) {
  return object.is_object() && object.contains(key);
}
const Json& field(const Json& object, const char* key) {
  return value::property(object, key);
}
void require(bool available, const char* name) {
  if (!available) throw std::runtime_error(std::string("Missing request dependency: ") + name);
}
Json numberValue(double value) {
  if (std::isfinite(value) && std::trunc(value) == value &&
      value >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
      value < static_cast<double>(std::numeric_limits<std::int64_t>::max()))
    return static_cast<std::int64_t>(value);
  return value;
}
} // namespace

Json getExtraBodyParams(const std::optional<std::string>& extraBodyStr, const Json& betaHeaders,
                        bool antiDistillationFeature, bool cliEntrypoint,
                        bool includeFirstPartyBetas, bool fakeGadgetInjectionEnabled,
                        const WireProtocol& wire, const Dependencies& deps) {
  Json result = Json::object();
  if (extraBodyStr && !extraBodyStr->empty()) {
    require(static_cast<bool>(deps.safeParseJSON), "safeParseJSON");
    try {
      Json parsed = deps.safeParseJSON(*extraBodyStr);
      if (parsed.is_object()) result = std::move(parsed);
      else {
        require(static_cast<bool>(deps.logForDebugging), "logForDebugging");
        deps.logForDebugging("BATBOX_EXTRA_BODY env var must be a JSON object, but was given " + *extraBodyStr);
      }
    } catch (const std::exception& error) {
      require(static_cast<bool>(deps.logForDebugging), "logForDebugging");
      deps.logForDebugging(std::string("Error parsing BATBOX_EXTRA_BODY: ") + error.what());
    }
  }
  if (antiDistillationFeature && cliEntrypoint && includeFirstPartyBetas && fakeGadgetInjectionEnabled)
    result["anti_distillation"] = Json::array({"fake_tools"});
  if (betaHeaders.is_array() && !betaHeaders.empty()) {
    require(!wire.betaHeadersKey.empty(), "WireProtocol.betaHeadersKey");
    if (result.contains(wire.betaHeadersKey) && result[wire.betaHeadersKey].is_array()) {
      const Json previous = result[wire.betaHeadersKey];
      Json existing = previous;
      for (const auto& header : betaHeaders) {
        if (std::find(previous.begin(), previous.end(), header) == previous.end()) existing.push_back(header);
      }
      result[wire.betaHeadersKey] = std::move(existing);
    } else result[wire.betaHeadersKey] = betaHeaders;
  }
  return result;
}

void configureEffortParams(const Json& effortValue, bool effortValueDefined,
                           Json& outputConfig, Json& extraBodyParams, Json& betas,
                           bool modelSupportsEffort, bool internalUser, const WireProtocol& wire) {
  if (!modelSupportsEffort || has(outputConfig, "effort")) return;
  if (!effortValueDefined) {
    require(!wire.effortBetaHeader.empty(), "WireProtocol.effortBetaHeader");
    betas.push_back(wire.effortBetaHeader);
  } else if (effortValue.is_string()) {
    require(!wire.effortBetaHeader.empty(), "WireProtocol.effortBetaHeader");
    outputConfig["effort"] = effortValue;
    betas.push_back(wire.effortBetaHeader);
  } else if (internalUser) {
    require(!wire.internalParamsKey.empty(), "WireProtocol.internalParamsKey");
    Json existing = field(extraBodyParams, wire.internalParamsKey.c_str());
    if (!value::truthy(existing)) existing = Json::object();
    // Preserve existing object fields while adding the override.
    if (!existing.is_object()) existing = Json::object();
    existing["effort_override"] = effortValue;
    extraBodyParams[wire.internalParamsKey] = std::move(existing);
  }
}

void configureMissionBudgetParams(const Json& missionBudget, Json& outputConfig, Json& betas,
                               bool includeFirstPartyBetas, const std::string& missionBudgetBetaHeader) {
  if (!value::truthy(missionBudget) || has(outputConfig, "task_budget") || !includeFirstPartyBetas) return;
  require(!missionBudgetBetaHeader.empty(), "WireProtocol.taskBudgetBetaHeader");
  Json budget = {{"type", "tokens"}};
  if (has(missionBudget, "total")) budget["total"] = missionBudget["total"];
  if (has(missionBudget, "remaining")) budget["remaining"] = missionBudget["remaining"];
  outputConfig["task_budget"] = std::move(budget);
  if (std::find(betas.begin(), betas.end(), Json(missionBudgetBetaHeader)) == betas.end()) betas.push_back(missionBudgetBetaHeader);
}

Json adjustParamsForNonStreaming(const Json& params, double maxTokensCap) {
  const double cappedMaxTokens = std::min(field(params, "max_tokens").get<double>(), maxTokensCap);
  Json adjustedParams = params;
  const Json& thinking = field(adjustedParams, "thinking");
  if (field(thinking, "type") == "enabled" && value::truthy(field(thinking, "budget_tokens"))) {
    Json adjustedThinking = thinking;
    adjustedThinking["budget_tokens"] = numberValue(std::min(field(thinking, "budget_tokens").get<double>(), cappedMaxTokens - 1));
    adjustedParams["thinking"] = std::move(adjustedThinking);
  }
  adjustedParams["max_tokens"] = numberValue(cappedMaxTokens);
  return adjustedParams;
}

} // namespace batbox::requests
