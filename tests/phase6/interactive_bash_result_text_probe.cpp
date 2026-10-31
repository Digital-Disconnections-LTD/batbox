#include "interactive/cards/BashResultText.hpp"
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
    const auto stderr = item.at("stderr").get<std::string>();
    const auto without = stderrWithoutSandboxViolations(stderr);
    const auto cwd = extractCwdResetWarning(without);
    output.push_back({{"name", item.at("name")},
      {"removed", removeSandboxViolationTags(stderr)},
      {"without", without}, {"cleaned", cwd.cleanedStderr},
      {"warning", cwd.cwdResetWarning ? Json(*cwd.cwdResetWarning) : Json(nullptr)}});
  }
  std::cout << output.dump() << '\n';
}
