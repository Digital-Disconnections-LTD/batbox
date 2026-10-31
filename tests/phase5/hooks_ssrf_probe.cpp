#include "instructions/hooks/runtime/SsrfGuard.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream stream(argv[1]);Json cases;stream>>cases;Json output=Json::array();
  for(const auto& value:cases){
    const auto address=value.get<std::string>();
    output.push_back({{"address",address},
      {"blocked",instructions::hooks::runtime::isBlockedHookAddress(address)}});
  }
  std::cout<<value::stringify(output)<<'\n';
}
