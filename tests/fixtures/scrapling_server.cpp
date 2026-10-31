// Offline MCP fixture for platform smoke checks. No browser or account is used.
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

int main() {
  using Json = nlohmann::json;
  for (std::string line; std::getline(std::cin, line);) {
    const auto request = Json::parse(line);
    if (!request.contains("id")) continue;
    const auto method = request.value("method", "");
    Json result = Json::object();
    if (method == "initialize") {
      result = {{"protocolVersion", "2025-03-26"},
                {"serverInfo", {{"name", "batbox-fixture"}, {"version", "1"}}},
                {"capabilities", {{"tools", Json::object()}}}};
    } else if (method == "tools/list") {
      result = {{"tools", Json::array()}};
    } else if (method == "tools/call") {
      result = {{"content", Json::array({{{"type", "text"}, {"text", "fixture"}}})}};
    }
    std::cout << Json({{"jsonrpc", "2.0"}, {"id", request.at("id")},
                       {"result", result}}).dump() << std::endl;
  }
}
