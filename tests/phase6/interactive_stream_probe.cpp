#include "interactive/StreamReducer.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace ix = batbox::interactive;
using batbox::Json;

static Json gadgetsJson(const std::vector<ix::StreamingGadgetUse>& values) {
  Json result = Json::array();
  for (const auto& item : values)
    result.push_back(Json{{"index", item.index},
      {"contentBlock", item.contentBlock},
      {"unparsedToolInput", item.unparsedGadgetInput}});
  return result;
}

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases")
    throw std::runtime_error("Usage: probe --cases path");
  std::ifstream input(argv[2]);
  if (!input) throw std::runtime_error("Cannot open stream cases");
  const auto cases = Json::parse(input);
  Json results = Json::array();
  for (const auto& spec : cases) {
    Json state{{"messages", Json::array()}, {"tombstones", Json::array()},
      {"mode", nullptr}, {"length", ""}, {"streamingText", nullptr},
      {"streamingToolUses", Json::array()}, {"thinking", nullptr},
      {"ttft", Json::array()}, {"trace", Json::array()}};
    std::vector<ix::StreamingGadgetUse> gadgets;
    std::optional<ix::StreamingThinking> thinking;
    std::optional<std::string> text;
    ix::StreamImports imports{
      [&]{ return spec.value("connectorTextEnabled", false); },
      [](const Json& block) {return block.is_object() &&
        block.value("type", "") == "text" &&
        block.value("isConnectorText", false);},
      [&]{ return spec.value("now", 5000.0); }};
    ix::StreamCallbacks callbacks;
    callbacks.onMessage = [&](const Json& value) {
      state["trace"].push_back(Json::array({"message", value.value("type", "")}));
      state["messages"].push_back(value);
    };
    callbacks.onUpdateLength = [&](const std::string& value) {
      state["trace"].push_back(Json::array({"length", value}));
      state["length"] = state["length"].get<std::string>() + value;
    };
    callbacks.onSetStreamMode = [&](const std::string& value) {
      state["trace"].push_back(Json::array({"mode", value}));
      state["mode"] = value;
    };
    callbacks.onStreamingGadgetUses = [&](ix::GadgetUsesUpdate update) {
      gadgets = update(gadgets);
      state["streamingToolUses"] = gadgetsJson(gadgets);
      state["trace"].push_back(Json::array({"tools", state["streamingToolUses"]}));
    };
    callbacks.onTombstone = [&](const Json& value) {
      state["trace"].push_back(Json::array({"tombstone", value.value("uuid", "")}));
      state["tombstones"].push_back(value);
    };
    callbacks.onStreamingThinking = [&](ix::ThinkingUpdate update) {
      thinking = update(thinking);
      if (thinking)
        state["thinking"] = Json{{"thinking", thinking->thinking},
          {"isStreaming", thinking->isStreaming},
          {"streamingEndedAt", thinking->streamingEndedAt.value()}};
      else state["thinking"] = nullptr;
      state["trace"].push_back(Json::array({"thinking", state["thinking"]}));
    };
    callbacks.onApiMetrics = [&](double value) {
      state["trace"].push_back(Json::array({"metrics", Json{{"ttftMs", value}}}));
      state["ttft"].push_back(value);
    };
    callbacks.onStreamingText = [&](ix::TextUpdate update) {
      text = update(text);
      state["streamingText"] = text ? Json(*text) : Json(nullptr);
      state["trace"].push_back(Json::array({"text", state["streamingText"]}));
    };
    for (const auto& event : spec.at("events"))
      ix::handleMessageFromStream(event, callbacks, imports);
    state["name"] = spec.at("name");
    results.push_back(std::move(state));
  }
  std::cout << results.dump() << '\n';
}
