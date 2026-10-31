#include "streaming/Reducer.hpp"
#include <cassert>

using batbox::Json;
using batbox::streaming::Reducer;
using batbox::streaming::ReducerConfig;

int main() {
  ReducerConfig config;
  config.randomUUID = [] { return "uuid-1"; };
  config.timestamp = [] { return "2026-10-28T12:00:00.000Z"; };
  config.nowMillis = [] { return std::int64_t{100}; };
  config.calculateUSDCost = [](const std::string&, const Json&) { return 0.0; };
  config.addToTotalFlightCost = [](double, const Json&, const std::string&) { return 0.0; };
  config.getErrorMessageIfRefusal = [](const Json&, const std::string&) -> std::optional<Json> { return std::nullopt; };
  config.createAssistantAPIErrorMessage = [](const std::string&, const std::string&, const std::string&) { return Json::object(); };
  Reducer reducer(std::move(config));
  reducer.consume({{"type", "message_start"}, {"message", {
    {"type", "message"}, {"content", Json::array()}, {"stop_reason", nullptr},
    {"usage", {{"input_tokens", 10}, {"output_tokens", 0}}}}}});
  reducer.consume({{"type", "content_block_start"}, {"index", 0},
                   {"content_block", {{"type", "text"}, {"text", ""}}}});
  auto yielded = reducer.consume({{"type", "content_block_stop"}, {"index", 0}});
  auto retained = yielded.at(0);
  assert(retained == reducer.messages().at(0));
  assert((*retained)["message"]["stop_reason"].is_null());
  reducer.consume({{"type", "message_delta"}, {"delta", {{"stop_reason", "end_turn"}}},
                   {"usage", {{"input_tokens", 0}, {"output_tokens", 4}}}});
  assert(retained == reducer.messages().at(0));
  assert((*retained)["message"]["stop_reason"] == "end_turn");
  assert((*retained)["message"]["usage"]["input_tokens"] == 10);
  assert((*retained)["message"]["usage"]["output_tokens"] == 4);
}
