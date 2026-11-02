// Offline MCP fixture for platform smoke checks. No browser or account is used.
// It advertises a real Scrapling tool set: --tools=0.4.7 (the default, `get`),
// --tools=0.4.15 (`make_request`) or --tools=unknown (no plain fetch tool).
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {
using Json = nlohmann::json;
Json pageSchema() {
  return {{"type", "object"}, {"required", {"url"}},
          {"properties",
           {{"url", {{"type", "string"}, {"title", "Url"}}},
            {"extraction_type",
             {{"type", "string"}, {"default", "markdown"}, {"enum", {"text", "html", "markdown"}}}},
            {"css_selector", {{"anyOf", {{{"type", "string"}}, {{"type", "null"}}}}, {"default", nullptr}}},
            {"main_content_only", {{"type", "boolean"}, {"default", true}}}}}};
}
std::vector<std::string> toolNames(const std::string& set) {
  if (set == "unknown") return {"browser_open", "browser_fetch", "browser_screenshot", "close_session"};
  std::vector<std::string> names{"open_session", "close_session", "list_sessions"};
  if (set == "0.4.15") names.insert(names.end(), {"open_request_session", "make_request"});
  else names.push_back("get");
  names.insert(names.end(), {"bulk_get", "fetch", "bulk_fetch", "stealthy_fetch",
                             "bulk_stealthy_fetch", "screenshot"});
  if (set == "0.4.15") names.insert(names.end(), {"session_fetch", "session_make_request"});
  return names;
}
Json textResult(const std::string& text, bool error = false) {
  return {{"content", Json::array({{{"type", "text"}, {"text", text}}})}, {"isError", error}};
}
} // namespace

int main(int argc, char** argv) {
  std::string set = "0.4.7";
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument.rfind("--tools=", 0) == 0) set = argument.substr(8);
  }
  const auto names = toolNames(set);
  int lists = 0;
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
      ++lists;
      Json tools = Json::array();
      for (const auto& name : names) {
        const bool page = name == "get" || name == "make_request" || name == "fetch" ||
                          name == "stealthy_fetch";
        tools.push_back({{"name", name}, {"title", name},
                         {"inputSchema", page ? pageSchema() : Json{{"type", "object"}}}});
      }
      result = {{"tools", tools}};
    } else if (method == "tools/call") {
      const auto name = request.at("params").value("name", "");
      const auto arguments = request.at("params").value("arguments", Json::object());
      if (name == "fixture_tools_list_count") {
        result = textResult(std::to_string(lists));
      } else if (std::find(names.begin(), names.end(), name) == names.end()) {
        result = textResult("Unknown tool: " + name, true);
      } else if (name == "get" || name == "make_request") {
        const auto url = arguments.value("url", "");
        if (url.find("error.example") != std::string::npos) {
          result = textResult("Failed to fetch " + url + ": connection refused", true);
        } else {
          const Json page = {{"status", 200},
                             {"content", {"# Fixture page", "Fetched with " + name + " as " +
                                          arguments.value("extraction_type", "default")}},
                             {"url", url}};
          result = textResult(page.dump(2));
          result["structuredContent"] = page;
        }
      } else {
        result = textResult("fixture");
      }
    }
    std::cout << Json({{"jsonrpc", "2.0"}, {"id", request.at("id")},
                       {"result", result}}).dump() << std::endl;
  }
}
