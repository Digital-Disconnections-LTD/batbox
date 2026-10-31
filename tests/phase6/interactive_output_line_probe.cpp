#include "interactive/cards/OutputLineCard.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
using batbox::Json;
using namespace batbox::interactive::cards;

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases") return 2;
  std::ifstream input(argv[2]);
  Json output = Json::array();
  for (const auto& item : Json::parse(input)) {
    OutputLineImports imports;
    imports.stringWidth = [](const std::string& text) {
      return static_cast<double>(text.size());
    };
    imports.sliceAnsi = [](const std::string& value, double start, double end) {
      return value.substr(static_cast<std::size_t>(start),
                          static_cast<std::size_t>(end - start));
    };
    imports.dim = [](const std::string& value) { return "<dim:" + value + ">"; };
    imports.expandHint = [] { return "(expand)"; };
    imports.createHyperlink = [](const std::string& url) {
      return "LINK(" + url + ")";
    };
    OutputLineOptions options;
    options.columns = item.value("columns", 80);
    options.verbose = item.value("verbose", false);
    options.expandShellOutput = item.value("expand", false);
    options.inVirtualList = item.value("virtual", false);
    options.linkifyUrls = item.value("linkify", false);
    options.isError = item.value("error", false);
    options.isWarning = item.value("warning", false);
    const auto line = outputLineCard(item.at("content").get<std::string>(),
                                      options, imports);
    output.push_back({{"name", item.at("name")}, {"formatted", line.formatted},
      {"color", line.colorRole ? Json(*line.colorRole) : Json(nullptr)}});
  }
  std::cout << output.dump() << '\n';
}
