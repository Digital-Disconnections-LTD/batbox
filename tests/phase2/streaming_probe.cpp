#include "streaming/Reducer.hpp"
#include <iostream>
#include <memory>

using batbox::Json;
using batbox::streaming::Reducer;
using batbox::streaming::ReducerConfig;
using batbox::streaming::StreamError;

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    try {
      const Json input = batbox::value::parse(line);
      ReducerConfig config;
      config.model = input.value("model", "");
      config.resolvedModel = input.value("resolvedModel", "");
      config.requestId = input.value("requestId", "");
      config.advisorModel = input.value("advisorModel", "");
      config.maxOutputTokens = input.value("maxOutputTokens", 0);
      config.startMillis = input.value("startMillis", std::int64_t{0});
      config.connectorTextEnabled = input.value("connectorTextEnabled", false);
      config.stashedMicrocompact = input.value("cachedMicrocompact", false);
      config.nowMillis = [] { return std::int64_t{1793188800000}; };
      int uuidCounter = 0;
      config.randomUUID = [&] { return "uuid-" + std::to_string(++uuidCounter); };
      config.timestamp = [] { return "2026-10-28T12:00:00.000Z"; };
      config.calculateUSDCost = [](const std::string&, const Json& usage) {
        return usage.at("output_tokens").get<double>() / 1000;
      };
      config.addToTotalFlightCost = [](double cost, const Json&, const std::string&) { return cost + 0.5; };
      config.getErrorMessageIfRefusal = [](const Json& reason, const std::string& model) -> std::optional<Json> {
        if (reason == "refusal") return Json{{"type", "refusal"}, {"model", model}};
        return std::nullopt;
      };
      config.createAssistantAPIErrorMessage = [](const std::string& content,
                                                    const std::string& apiError,
                                                    const std::string& error) {
        return Json{{"type", "error"}, {"content", content}, {"apiError", apiError}, {"error", error}};
      };
      Reducer reducer(std::move(config));
      Json emitted = Json::array();
      Json output;
      try {
        for (const Json& part : input.value("events", Json::array())) {
          for (const auto& event : reducer.consume(part)) emitted.push_back(*event);
        }
        if (!input.value("skipFinish", false)) reducer.finish();
        Json messages = Json::array();
        for (const auto& message : reducer.messages()) messages.push_back(*message);
        Json blocks = Json::array();
        for (const auto& block : reducer.contentBlocks()) blocks.push_back(block ? *block : Json(nullptr));
        output = {{"emitted", emitted}, {"state", {
          {"newMessages", messages}, {"usage", reducer.usage()},
          {"stopReason", reducer.stopReason()}, {"costUSD", reducer.costUSD()},
          {"ttftMs", reducer.ttftMs()}, {"isAdvisorInProgress", reducer.advisorInProgress()},
          {"contentBlocks", blocks}}}};
      } catch (const StreamError& error) {
        output = {{"emitted", emitted}, {"error", {{"name", error.name()}, {"message", error.what()}}}};
      } catch (const std::exception& error) {
        output = {{"emitted", emitted}, {"error", {{"name", "Error"}, {"message", error.what()}}}};
      }
      std::cout << batbox::value::stringify(output) << '\n';
    } catch (const std::exception& error) {
      std::cout << batbox::value::stringify(Json{{"fatal", error.what()}}) << '\n';
    }
  }
}
