#include "interactive/cards_gadget_result/HookProgressCard.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>

using namespace batbox::interactive;
using namespace batbox::interactive::cards_gadget_result;
using batbox::Json;

int main(int argc,char** argv){
  if(argc!=3||std::string(argv[1])!="--cases")return 2;
  std::ifstream stream(argv[2]);
  Json output=Json::array();
  for(const auto& item:Json::parse(stream)){
    MessageDecisionLookups lookups;
    const auto event=item.at("event").get<std::string>();
    if(const auto value=item.at("inProgress").get<int>();value)
      lookups.inProgressHookCounts["tool"][event]=value;
    if(const auto value=item.at("resolved").get<int>();value)
      lookups.resolvedHookCounts["tool"][event]=value;
    const auto card=classifyHookProgress(event,"tool",
      item.at("transcript").get<bool>(),lookups);
    output.push_back({{"name",item.at("name")},
      {"kind",hookProgressKindName(card.kind)},
      {"text",card.text}});
  }
  std::cout<<output.dump()<<'\n';
}
