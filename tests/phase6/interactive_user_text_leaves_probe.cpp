#include "interactive/cards/UserTextLeaves.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
using batbox::interactive::cards::bashInputLeaf;
using batbox::interactive::cards::bashOutputLeaf;
using batbox::interactive::cards::commandLeaf;
using batbox::Json;

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases") return 2;
  std::ifstream input(argv[2]);
  const auto cases = Json::parse(input);
  Json result = Json::array();
  for (const auto& spec : cases) {
    const auto name = spec.at("name");
    const auto kind = spec.at("kind").get<std::string>();
    const auto text = spec.at("text").get<std::string>();
    const bool margin = spec.value("margin", false);
    Json value;
    if (kind == "input") {
      const auto leaf = bashInputLeaf(text, margin);
      if (leaf) value = Json{{"text", "! " + leaf->input},
                             {"margin", margin ? 1 : 0},
                             {"background", "bashMessageBackgroundColor"}};
    } else if (kind == "output") {
      const auto leaf = bashOutputLeaf(text, spec.value("verbose", false));
      value = Json{{"stdout", leaf.stdoutText}, {"stderr", leaf.stderrText},
                   {"verbose", leaf.verbose}};
    } else {
      const auto leaf = commandLeaf(text, "command-message", margin);
      if (leaf) value = Json{{"text", "❯ " + leaf->label},
                             {"margin", margin ? 1 : 0},
                             {"background", "userMessageBackground"}};
    }
    result.push_back({{"name", name}, {"kind", kind}, {"value", value}});
  }
  std::cout << result.dump() << '\n';
}
