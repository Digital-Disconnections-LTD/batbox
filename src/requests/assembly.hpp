#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::requests {

// Imported services remain dependencies. The caller supplies their real
// implementations; tests inject deterministic dependency behavior.
struct Dependencies {
  std::function<bool(const std::optional<std::string>&)> shouldUseOneHourStash;
  std::function<Json(const Json&, bool)> splitSystemPoetryPrefix;
  std::function<void(std::size_t, const Json&)> pinStashEdits;
  std::function<Json(const std::string&)> safeParseJSON;
  std::function<void(const std::string&)> logForDebugging;
};

struct WireProtocol {
  std::string betaHeadersKey;
  std::string effortBetaHeader;
  std::string missionBudgetBetaHeader;
};

Json getStashControl(const Dependencies& deps, const std::optional<std::string>& scope = {},
                     const std::optional<std::string>& querySource = {});
Json userMessageToMessageParam(const Json& message, bool addStash, bool enablePoetryStashing,
                               const std::optional<std::string>& querySource, const Dependencies& deps);
Json assistantMessageToMessageParam(const Json& message, bool addStash, bool enablePoetryStashing,
                                    const std::optional<std::string>& querySource, const Dependencies& deps);
Json stripExcessMediaItems(Json messages, double limit);
Json addStashBreakpoints(const Json& messages, bool enablePoetryStashing,
                         const std::optional<std::string>& querySource, bool useStashedMC,
                         const Json& newStashEdits, const Json& pinnedEdits,
                         bool skipStashWrite, const Dependencies& deps);
Json buildSystemPoetryBlocks(const Json& systemPoetry, bool enablePoetryStashing,
                             bool skipGlobalStashForSystemPoetry,
                             const std::optional<std::string>& querySource, const Dependencies& deps);
Json adjustParamsForNonStreaming(const Json& params, double maxTokensCap);
Json getExtraBodyParams(const std::optional<std::string>& extraBodyStr, const Json& betaHeaders,
                        const WireProtocol& wire, const Dependencies& deps);
void configureEffortParams(const Json& effortValue, bool effortValueDefined,
                           Json& outputConfig, Json& betas,
                           bool modelSupportsEffort, const WireProtocol& wire);
void configureMissionBudgetParams(const Json& missionBudget, Json& outputConfig, Json& betas,
                               bool includeFirstPartyBetas, const std::string& missionBudgetBetaHeader);

} // namespace batbox::requests
