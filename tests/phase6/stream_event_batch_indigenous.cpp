#include "streaming/Reducer.hpp"
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using batbox::Json;
namespace {
void check(bool condition, const std::string& what) {
  if (!condition) throw std::runtime_error("check failed: " + what);
}

batbox::streaming::ReducerConfig reducerConfig() {
  batbox::streaming::ReducerConfig config;
  int uuid = 0;
  config.randomUUID = [uuid]() mutable { return "uuid-" + std::to_string(++uuid); };
  config.timestamp = [] { return "2026-11-05T00:00:00.000Z"; };
  config.nowMillis = [] { return std::int64_t{100}; };
  config.calculateUSDCost = [](const std::string&, const Json&) { return 0.5; };
  config.addToTotalFlightCost = [](double cost, const Json&, const std::string&) { return cost; };
  config.getErrorMessageIfRefusal = [](const Json& reason, const std::string&) -> std::optional<Json> {
    if (reason == "refusal") return Json{{"type", "assistant"}, {"refused", true}};
    return std::nullopt;
  };
  config.createAssistantAPIErrorMessage = [](const std::string& text, const std::string& error, const std::string&) {
    return Json{{"type", "assistant"}, {"error", error}, {"text", text}};
  };
  config.maxOutputTokens = 64;
  return config;
}
std::vector<Json> streamEvents(const std::string& stopReason) {
  std::vector<Json> events = {
    {{"type", "message_start"}, {"message", {{"id", "msg_1"}, {"type", "message"}, {"role", "assistant"}, {"content", Json::array()},
      {"stop_reason", nullptr}, {"usage", {{"input_tokens", 10}, {"output_tokens", 0}}}}}},
    {{"type", "content_block_start"}, {"index", 0}, {"content_block", {{"type", "text"}, {"text", ""}}}},
  };
  for (int i = 0; i < 5; ++i)
    events.push_back({{"type", "content_block_delta"}, {"index", 0}, {"delta", {{"type", "text_delta"}, {"text", "word " + std::to_string(i)}}}});
  events.push_back({{"type", "content_block_stop"}, {"index", 0}});
  events.push_back({{"type", "message_delta"}, {"delta", {{"stop_reason", stopReason}}}, {"usage", {{"output_tokens", 7}}}});
  events.push_back({{"type", "message_stop"}});
  return events;
}
void reducerBatchesMatchConsume() {
  for (const std::string stop : {"end_turn", "max_tokens", "refusal", "model_context_window_exceeded"}) {
    batbox::streaming::Reducer whole(reducerConfig()), batched(reducerConfig());
    std::vector<std::string> expected, actual;
    std::size_t lazy = 0;
    for (const auto& event : streamEvents(stop)) {
      for (const auto& emission : whole.consume(event)) expected.push_back(batbox::value::stringify(*emission));
      auto batch = batched.beginEventBatch(event);
      for (const auto& emission : batch.ready) actual.push_back(batbox::value::stringify(*emission));
      if (event.at("type") != "message_delta") check(!batch.more, "buffered events come back whole");
      if (batch.more) {
        ++lazy;
        while (auto emission = batched.nextEmission()) actual.push_back(batbox::value::stringify(**emission));
      }
    }
    check(lazy == 1, "only message_delta resumes on demand");
    check(expected == actual, "batched emissions equal consume() for stop reason " + stop);
    check(batbox::value::stringify(whole.usage()) == batbox::value::stringify(batched.usage()), "usage matches");
  }
}

} // namespace

int main() {
  reducerBatchesMatchConsume();
  std::cout << "stream event batches: emissions match consume() for every stop reason\n";
  return 0;
}
