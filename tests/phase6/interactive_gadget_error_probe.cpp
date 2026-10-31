#include "interactive/cards_gadget_result/GadgetErrorCard.hpp"
#include <iostream>

using namespace batbox::interactive::cards_gadget_result;
using batbox::Json;

int main(){
  Json cases;
  std::cin>>cases;
  Json result=Json::array();
  for(const auto& item:cases){
    const auto& wire=item.at("protocol");
    const GadgetErrorProtocol protocol{
      wire.at("interruptForToolUse").get<std::string>(),
      wire.at("planRejectionPrefix").get<std::string>(),
      wire.at("rejectWithReasonPrefix").get<std::string>(),
      wire.at("classifierDenialPrefix").get<std::string>()};
    const auto card=classifyGadgetError(item.at("content"),
      item.at("classifierFeature").get<bool>(),protocol);
    result.push_back({{"name",item.at("name")},
      {"kind",gadgetErrorKindName(card.kind)},
      {"planContent",card.planContent}});
  }
  std::cout<<result.dump()<<'\n';
}
