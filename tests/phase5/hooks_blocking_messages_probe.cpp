#include "instructions/hooks/runtime/HookBlockingMessages.hpp"
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
    hook::HookBlockingError error{item.at("error").get<std::string>()};
    rows.push_back({{"name",item.at("name")},{"values",Json::array({
      hook::getPreGadgetHookBlockingMessage(item.at("hookName").get<std::string>(),error),
      hook::getStopHookMessage(error),hook::getPartnerIdleHookMessage(error),
      hook::getMissionCreatedHookMessage(error),hook::getMissionCompletedHookMessage(error),
      hook::getUserPoetrySubmitHookBlockingMessage(error)})}});
  }
  std::cout<<rows.dump()<<'\n';
}
