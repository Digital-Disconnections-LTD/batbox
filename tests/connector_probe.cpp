#include "connectors/Scrapling.hpp"
#include "connectors/Searxng.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace batbox;
int main(int argc, char** argv) {
  try {
    if (argc < 3) throw std::invalid_argument("Usage: batbox-connectors search QUERY | fetch URL | session JSONL; configure BATBOX_SCRAPLING_EXECUTABLE");
    const std::string op = argv[1];
    if (op == "search") {
      auto* endpoint = std::getenv("BATBOX_SEARXNG_URL");
      Json input = {{"query", argv[2]}};
      if (argc > 3) input = Json::parse(argv[3]);
      std::cout << Searxng(endpoint ? endpoint : "http://127.0.0.1:8888/search").search(input,"batbox-search").dump() << '\n';
      return 0;
    }
    auto* executable = std::getenv("BATBOX_SCRAPLING_EXECUTABLE");
    if (!executable) throw std::invalid_argument("Set BATBOX_SCRAPLING_EXECUTABLE to the Scrapling MCP server");
    auto* wait = std::getenv("BATBOX_CONNECTOR_TIMEOUT_MS");
    Scrapling flight({executable}, std::chrono::milliseconds(wait ? std::stoll(wait) : 7200000));
    if (op == "fetch") std::cout << flight.fetch(argv[2]).dump() << '\n';
    else if (op == "session") {
      // Multiple calls in one indigenous process retain live browser/flight IDs.
      for (std::string line; std::getline(std::cin,line);) {
        const auto call = Json::parse(line);
        auto result = call.at("name") == "tools/list" ? flight.listGadgets() : flight.callGadget(call.at("name"),call.value("arguments",Json::object()));
        std::cout << result.dump() << std::endl;
      }
    } else throw std::invalid_argument("Unknown connector operation");
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
