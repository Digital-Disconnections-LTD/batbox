#pragma once

#include "requests/assembly.hpp"
#include <functional>

namespace batbox::requests {

// Captured values initialized before assembling request parameters.
struct RequestCapture {
  Json betas = Json::array();
  Json messagesForAPI = Json::array();
  Json system = Json::array();
  Json allGadgets = Json::array();
  Json options = Json::object();
  Json thinkingConfig = Json::object();
  Json consumedStashEdits;
  Json consumedPinnedEdits = Json::array();
  Json effort;
  bool effortDefined = false;
  bool stashedMCEnabled = false;
  bool thinkingClearLatched = false;
  bool fastModeHeaderLatched = false;
  bool stashEditingHeaderLatched = false;
  bool includeFirstPartyBetas = false;
  bool disableThinking = false;
  bool disableAdaptiveThinking = false;
  std::string provider;
  std::string gadgetSearchHeader;
  std::string stashEditingBetaHeader;
  std::optional<std::string> extraBodyStr;
  Json lastRequestBetas = Json::array();
  // Tokens the conversation is known to hold from the last response's usage;
  // 0 when no response reported any.
  double knownPromptTokens = 0;
  // Set by each build: max_tokens was lowered so the request fits the
  // context window.
  bool outputCapFromContext = false;
};

struct BuilderDependencies {
  Dependencies assembly;
  std::function<bool(const std::string&)> getLargeContextTreatment;
  std::function<Json(const std::string&)> getProviderExtraBodyBetas;
  std::function<bool(const std::string&)> modelSupportsEffort;
  std::function<bool(const std::string&)> modelSupportsStructuredOutputs;
  std::function<Json(const std::string&)> getMaxOutputTokensForModel;
  // The endpoint's context window; unbound or 0: max_tokens is not fitted.
  std::function<double(const std::string&)> getContextWindowForModel;
  std::function<bool(const std::string&)> modelSupportsThinking;
  std::function<bool(const std::string&)> modelSupportsAdaptiveThinking;
  std::function<Json(const std::string&)> getMaxThinkingTokensForModel;
  std::function<Json(const Json&)> getAPIContextManagement;
  std::function<bool(const std::string&)> getPoetryStashingEnabled;
  std::function<bool()> isFastModeEnabled;
  std::function<bool()> isFastModeAvailable;
  std::function<bool()> isFastModeCooldown;
  std::function<bool(const std::string&)> isFastModeSupportedByModel;
  std::function<std::string(const std::string&)> normalizeModelStringForAPI;
};

struct BuilderHeaders {
  std::string largeContext;
  std::string structuredOutputs;
  std::string redactThinking;
  std::string fastMode;
  std::string contextManagement;
};

Json buildParamsFromContext(const Json& retryContext, RequestCapture& capture,
                            const BuilderDependencies& deps, const WireProtocol& wire,
                            const BuilderHeaders& headers);

} // namespace batbox::requests
