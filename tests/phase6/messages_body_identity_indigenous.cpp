#include "transport/Messages.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using batbox::Json;
namespace {
void check(bool condition, const std::string& what) {
  if (!condition) throw std::runtime_error("check failed: " + what);
}

std::string outcome(const std::function<std::string()>& run) {
  try { return run(); } catch (const std::exception& error) { return std::string("error: ") + error.what(); }
}
void messagesBodyMatchesPreparedRequest() {
  const Json overrides = {{"x-extra", "1"}};
  const std::vector<Json> cases = {
    {{"model", "m"}, {"messages", Json::array({Json{{"role", "user"}, {"content", "naïve \"quote\" 日本\n"}}})}, {"max_tokens", 5}},
    {{"model", "m"}, {"betas", Json::array({"a", "b"})}, {"user_profile_id", "u"}, {"workspace_id", "w"}, {"system", "s"}},
    {{"stream", false}, {"model", "m"}, {"messages", Json::array()}, {"temperature", 0.5}},
    {{"model", "m"}, {"10", 1}, {"2", 2}, {"metadata", {{"user_id", "x"}}}},
    {{"model", "m"}, {"output_format", {{"type", "json_schema"}}}, {"output_config", {{"effort", "high"}}}},
    {{"model", "m"}, {"output_format", {{"type", "json_schema"}}}, {"output_config", {{"format", {{"type", "x"}}}}}},
    Json::array({1, 2}), Json(nullptr),
  };
  for (const auto& params : cases)
    for (const bool stream : {true, false}) {
      const auto expected = outcome([&] {
        auto prepared = batbox::transport::prepareMessageRequest(params, overrides);
        prepared.body["stream"] = stream;
        return batbox::value::stringify(prepared.body) + "|" + batbox::value::stringify(prepared.headers);
      });
      const auto actual = outcome([&] {
        auto serialized = batbox::transport::serializeMessageRequest(params, stream, overrides);
        return serialized.body + "|" + batbox::value::stringify(serialized.headers);
      });
      check(expected == actual, "Messages body for " + params.dump());
    }
}

} // namespace

int main() {
  messagesBodyMatchesPreparedRequest();
  std::cout << "Messages body: serialized request matches the prepared request\n";
  return 0;
}
