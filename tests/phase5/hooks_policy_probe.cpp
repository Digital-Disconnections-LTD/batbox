#include "instructions/hooks/runtime/HookPolicyLeaves.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    Json calls=Json::array();
    std::optional<std::string> raw;
    if(!item.at("raw").is_null())raw=item.at("raw").get<std::string>();
    const auto timeout=hook::getFlightEndHookTimeoutMs(raw);
    const auto skip=hook::shouldSkipHookDueToTrust(
      [&]{calls.push_back("noninteractive");return item.at("nonInteractive").get<bool>();},
      [&]{calls.push_back("trust");return item.at("trusted").get<bool>();});
    rows.push_back({{"name",item.at("name")},{"timeout",timeout},
      {"skip",skip},{"calls",calls}});
  }
  std::cout<<value::stringify(rows)<<'\n';
}
