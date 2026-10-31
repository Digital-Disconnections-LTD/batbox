#include "interactive/text_input/KillRing.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using batbox::interactive::text_input::KillRing;
using Json=nlohmann::ordered_json;
int main(){
  KillRing k;Json out=Json::array();
  auto snapshot=[&]{out.push_back(Json::array(
    {k.size(),k.last(),k.item(-1),k.canYankPop()}));};
  auto pop=[&]{
    const auto item=k.yankPop();
    out.push_back(item?Json{{"text",item->text},{"start",item->start},
      {"length",item->length}}:Json(nullptr));
  };
  k.clear();snapshot();
  k.push("abc");snapshot();
  k.push("def");snapshot();
  k.resetKillAccumulation();k.push("ghi");snapshot();
  k.recordYank(2,3);snapshot();
  pop();k.updateYankLength(6);pop();k.resetYankState();pop();
  k.resetKillAccumulation();k.push("Z",KillRing::Direction::Prepend);snapshot();
  k.push("Q",KillRing::Direction::Prepend);snapshot();
  for(int i=0;i<13;++i){k.resetKillAccumulation();k.push(std::to_string(i));}
  snapshot();k.clear();snapshot();
  std::cout<<out.dump()<<'\n';
}
