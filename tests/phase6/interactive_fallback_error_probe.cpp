#include "interactive/cards_gadget_result/FallbackErrorCard.hpp"
#include <iostream>

using namespace batbox::interactive::cards_gadget_result;
using batbox::Json;

int main(){
  Json cases;
  std::cin>>cases;
  Json result=Json::array();
  for(const auto& item:cases){
    const auto card=fallbackErrorCard(item.at("result"),
      item.at("verbose").get<bool>(),[] {return "ctrl+o";});
    const auto hint=card.showExpand?
      "… +"+std::to_string(card.extraLines)+
      (card.extraLines==1?" line (":" lines (")+
      card.shortcut+" to see all)":std::string{};
    result.push_back({{"name",item.at("name")},
      {"errorText",card.errorText},{"hintText",hint}});
  }
  std::cout<<result.dump()<<'\n';
}
