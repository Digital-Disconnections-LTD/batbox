#include "requests/assembly.hpp"
#include <iostream>
#include <optional>
#include <string>

using batbox::Json;
namespace request = batbox::requests;

static std::optional<std::string> optionalString(const Json& input, const char* key) {
  if (!input.contains(key) || input[key].is_null()) return {};
  return input[key].get<std::string>();
}

static Json evaluate(const Json& input) {
  Json pins = Json::array();
  request::Dependencies deps;
  deps.shouldUseOneHourStash = [&input](const std::optional<std::string>& source) {
    if (!input.value("oneHour", false)) return false;
    if (!input.contains("oneHourSource")) return true;
    return source && *source == input["oneHourSource"].get<std::string>();
  };
  deps.splitSystemPoetryPrefix = [&input](const Json&, bool) { return input.at("splitBlocks"); };
  deps.connectorTextFeature = input.value("connectorTextFeature", false);
  deps.isConnectorTextBlock = [](const Json& block) { return block.is_object() && block.value("type", "") == "connector_text"; };
  deps.pinStashEdits = [&pins](std::size_t index, const Json& block) { pins.push_back(Json{{"index", index}, {"block", block}}); };
  deps.safeParseJSON = [](const std::string& value) {
    const std::string_view text = value.rfind("\xef\xbb\xbf", 0) == 0 ? std::string_view(value).substr(3) : std::string_view(value);
    try { return Json::parse(text); } catch (const Json::parse_error&) { return Json(); }
  };
  deps.logForDebugging = [](const std::string&) {};
  request::WireProtocol wire;
  if (input.contains("wire")) {
    const Json& keys = input["wire"];
    wire.betaHeadersKey = keys.value("betaHeadersKey", "");
    wire.internalParamsKey = keys.value("internalParamsKey", "");
    wire.effortBetaHeader = keys.value("effortBetaHeader", "");
    wire.missionBudgetBetaHeader = keys.value("taskBudgetBetaHeader", "");
  }
  const std::string op = input.at("op").get<std::string>();
  const bool stash = input.value("cache", false);
  const auto querySource = optionalString(input, "querySource");
  if (op == "user") return request::userMessageToMessageParam(input.at("message"), input.value("addCache", false), stash, querySource, deps);
  if (op == "assistant") return request::assistantMessageToMessageParam(input.at("message"), input.value("addCache", false), stash, querySource, deps);
  if (op == "strip") return request::stripExcessMediaItems(input.at("messages"), input.at("limit").get<double>());
  if (op == "breakpoints") {
    Json result = request::addStashBreakpoints(input.at("messages"), stash, querySource,
      input.value("useCachedMC", false), input.value("newCacheEdits", Json()),
      input.value("pinnedEdits", Json()), input.value("skipCacheWrite", false), deps);
    return Json{{"messages", result}, {"pins", pins}};
  }
  if (op == "system") return request::buildSystemPoetryBlocks(input.at("systemPrompt"), stash, input.value("skipGlobal", false), querySource, deps);
  if (op == "nonstream") return request::adjustParamsForNonStreaming(input.at("params"), input.at("cap").get<double>());
  if (op == "extraBody") return request::getExtraBodyParams(optionalString(input, "extraBody"), input.value("betaHeaders", Json()),
    input.value("antiDistillationFeature", false), input.value("cliEntrypoint", false),
    input.value("firstParty", false), input.value("fakeToolInjectionEnabled", false), wire, deps);
  if (op == "effort") {
    Json output = input.value("outputConfig", Json::object());
    Json extra = input.value("extraBodyParams", Json::object());
    Json betas = input.value("betas", Json::array());
    request::configureEffortParams(input.value("effortValue", Json()), input.contains("effortValue"),
      output, extra, betas, input.value("modelSupportsEffort", false), input.value("internalUser", false), wire);
    return Json{{"outputConfig", output}, {"extraBodyParams", extra}, {"betas", betas}};
  }
  if (op == "taskBudget") {
    Json output = input.value("outputConfig", Json::object());
    Json betas = input.value("betas", Json::array());
    request::configureMissionBudgetParams(input.value("taskBudget", Json()), output, betas,
      input.value("firstParty", false), wire.missionBudgetBetaHeader);
    return Json{{"outputConfig", output}, {"betas", betas}};
  }
  if (op == "cacheControl") return request::getStashControl(deps, optionalString(input, "scope"), querySource);
  throw std::runtime_error("Unknown request operation");
}

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    try { std::cout << Json{{"result", evaluate(Json::parse(line))}}.dump() << '\n'; }
    catch (const std::exception& error) { std::cout << Json{{"error", error.what()}}.dump() << '\n'; }
  }
}
