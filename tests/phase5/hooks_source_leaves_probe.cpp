#include "instructions/hooks/runtime/HookSourceLeaves.hpp"
#include <fstream>
#include <iostream>

using namespace batbox;
using namespace batbox::instructions::hooks::runtime;
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);Json cases;file>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    HookBatchMatch match;
    match.hook=std::make_shared<HookIdentity>(HookIdentity{item.at("hook")});
    std::vector<HookResult> results;
    for(const auto& result:item.at("results"))
      results.emplace_back("","",false,result.at("blocked").get<bool>());
    rows.push_back({{"name",item.at("name")},
      {"internal",isInternalHook(match)},
      {"blocked",hasBlockingResult(results)}});
  }
  std::cout<<rows.dump()<<'\n';
}
