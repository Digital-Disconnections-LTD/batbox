#include "interactive/cards_gadget_result/GadgetResultRouter.hpp"
#include <iostream>

using namespace batbox::interactive;
using namespace batbox::interactive::cards_gadget_result;
using batbox::Json;

int main(){
  Json cases;
  std::cin>>cases;
  Json result=Json::array();
  for(const auto& item:cases){
    MessageLookups lookups;
    batbox::gadgets::Gadget gadget;
    gadget.name="TestTool";
    batbox::gadgets::Gadgets gadgets;
    if(item.at("toolPresent").get<bool>())gadgets.push_back(&gadget);
    if(item.at("lookupPresent").get<bool>()){
      auto owner=std::make_shared<Json>(Json{{"type","assistant"},
        {"message",Json{{"content",Json::array({
          Json{{"type","tool_use"},{"id","tool"},
               {"name","TestTool"},{"input",Json::object()}}
        })}}}});
      lookups.gadgetUseByGadgetUseID["tool"]={owner,0};
    }
    const auto& wire=item.at("protocol");
    const GadgetResultProtocol protocol{
      wire.at("cancelMessage").get<std::string>(),
      wire.at("rejectMessage").get<std::string>(),
      wire.at("interruptForToolUse").get<std::string>()};
    const auto card=classifyGadgetResult(item.at("param"),lookups,gadgets,protocol);
    result.push_back({{"name",item.at("name")},
                      {"kind",gadgetResultKindName(card.kind)}});
  }
  std::cout<<result.dump()<<'\n';
}
