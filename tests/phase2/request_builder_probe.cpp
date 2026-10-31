#include "requests/RequestBuilder.hpp"
#include <iostream>
#include <string>

using batbox::Json;
namespace req = batbox::requests;

static req::RequestCapture captureFrom(const Json& c) {
  req::RequestCapture out;
  out.betas = c.at("betas"); out.messagesForAPI = c.at("messagesForAPI");
  out.system = c.at("system"); out.allGadgets = c.at("allTools");
  out.options = c.at("options"); out.thinkingConfig = c.at("thinkingConfig");
  out.consumedStashEdits = c.value("consumedCacheEdits", Json());
  out.consumedPinnedEdits = c.value("consumedPinnedEdits", Json::array());
  out.effort = c.value("effort", Json()); out.effortDefined = c.value("effortDefined", false);
  out.stashedMCEnabled = c.value("cachedMCEnabled", false);
  out.thinkingClearLatched = c.value("thinkingClearLatched", false);
  out.fastModeHeaderLatched = c.value("fastModeHeaderLatched", false);
  out.afkHeaderLatched = c.value("afkHeaderLatched", false);
  out.stashEditingHeaderLatched = c.value("cacheEditingHeaderLatched", false);
  out.isAgenticQuery = c.value("isAgenticQuery", false);
  out.transcriptClassifierFeature = c.value("transcriptClassifierFeature", false);
  out.includeFirstPartyBetas = c.value("includeFirstPartyBetas", false);
  out.internalUser = c.value("internalUser", false);
  out.disableThinking = c.value("disableThinking", false);
  out.disableAdaptiveThinking = c.value("disableAdaptiveThinking", false);
  out.provider = c.value("provider", "firstParty");
  out.gadgetSearchHeader = c.value("toolSearchHeader", "");
  out.stashEditingBetaHeader = c.value("cacheEditingBetaHeader", "");
  if (c.contains("extraBodyStr") && c["extraBodyStr"].is_string()) out.extraBodyStr = c["extraBodyStr"].get<std::string>();
  out.antiDistillationFeature = c.value("antiDistillationFeature", false);
  out.cliEntrypoint = c.value("cliEntrypoint", false);
  out.fakeGadgetInjectionEnabled = c.value("fakeToolInjectionEnabled", false);
  return out;
}

static Json evaluate(const Json& input) {
  req::RequestCapture capture = captureFrom(input.at("capture"));
  Json pins = Json::array();
  req::BuilderDependencies deps;
  deps.assembly.shouldUseOneHourStash = [&input](const std::optional<std::string>& source) {
    return input.value("oneHour", false) && (!input.contains("oneHourSource") ||
      (source && *source == input["oneHourSource"].get<std::string>()));
  };
  deps.assembly.connectorTextFeature = input.at("capture").value("connectorTextFeature", false);
  deps.assembly.isConnectorTextBlock = [](const Json& block) { return block.is_object() && block.value("type", "") == "connector_text"; };
  deps.assembly.pinStashEdits = [&pins](std::size_t index, const Json& block) { pins.push_back(Json{{"index", index}, {"block", block}}); };
  deps.assembly.safeParseJSON = [](const std::string& value) {
    const std::string_view text = value.rfind("\xef\xbb\xbf", 0) == 0 ? std::string_view(value).substr(3) : std::string_view(value);
    try { return Json::parse(text); } catch (const Json::parse_error&) { return Json(); }
  };
  deps.assembly.logForDebugging = [](const std::string&) {};
  deps.getLargeContextTreatment = [&input](const std::string&) { return input.value("largeContextTreatment", false); };
  deps.getProviderExtraBodyBetas = [&input](const std::string&) { return input.value("providerBetas", Json::array()); };
  deps.modelSupportsEffort = [&input](const std::string&) { return input.value("modelSupportsEffort", false); };
  deps.modelSupportsStructuredOutputs = [&input](const std::string&) { return input.value("modelSupportsStructuredOutputs", false); };
  deps.getMaxOutputTokensForModel = [&input](const std::string&) { return input.at("maxOutputDefault"); };
  deps.modelSupportsThinking = [&input](const std::string&) { return input.value("modelSupportsThinking", false); };
  deps.modelSupportsAdaptiveThinking = [&input](const std::string&) { return input.value("modelSupportsAdaptiveThinking", false); };
  deps.getMaxThinkingTokensForModel = [&input](const std::string&) { return input.at("maxThinkingDefault"); };
  deps.getAPIContextManagement = [&input](const Json&) { return input.value("contextManagement", Json()); };
  deps.getPoetryStashingEnabled = [&input](const std::string&) { return input.value("promptCachingDefault", false); };
  deps.isFastModeEnabled = [&input]() { return input.value("fastModeEnabled", false); };
  deps.isFastModeAvailable = [&input]() { return input.value("fastModeAvailable", false); };
  deps.isFastModeCooldown = [&input]() { return input.value("fastModeCooldown", false); };
  deps.isFastModeSupportedByModel = [&input](const std::string&) { return input.value("fastModeSupported", false); };
  deps.normalizeModelStringForAPI = [&input](const std::string& model) {
    return input.value("normalizedModel", model);
  };
  const Json& keys = input.at("wire");
  req::WireProtocol wire{
    keys.at("betaHeadersKey").get<std::string>(), keys.at("internalParamsKey").get<std::string>(),
    keys.at("effortBetaHeader").get<std::string>(), keys.at("taskBudgetBetaHeader").get<std::string>()};
  const Json& hs = input.at("headers");
  req::BuilderHeaders headers{
    hs.at("largeContext").get<std::string>(), hs.at("structuredOutputs").get<std::string>(),
    hs.at("redactThinking").get<std::string>(), hs.at("fastMode").get<std::string>(),
    hs.at("afkMode").get<std::string>(), hs.at("contextManagement").get<std::string>()};
  Json params;
  if (input.contains("retryContexts")) {
    params = Json::array();
    for (const auto& context : input["retryContexts"])
      params.push_back(req::buildParamsFromContext(context, capture, deps, wire, headers));
  } else params = req::buildParamsFromContext(input.at("retryContext"), capture, deps, wire, headers);
  return Json{{"params", params}, {"lastRequestBetas", capture.lastRequestBetas}, {"pins", pins}};
}

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    try { std::cout << Json{{"result", evaluate(Json::parse(line))}}.dump() << '\n'; }
    catch (const std::exception& error) { std::cout << Json{{"error", error.what()}}.dump() << '\n'; }
  }
}
