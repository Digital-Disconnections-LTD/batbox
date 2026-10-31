#include "monster/Monster.hpp"
#include <iostream>
#include <iterator>
using namespace batbox;
Json run(const Json& item){
 monster::Services s;s.getGlobalConfig=[&]{return item.value("config",Json::object());};
 if(item.contains("bunHash"))s.bunHash=[&](const std::string&){return std::stoull(item.at("bunHash").get<std::string>());};
 monster::Roller roller(s);Json out=Json::array();
 for(const auto& step:item.at("steps")){
  const auto op=step.at("op").get<std::string>();
  if(op=="roll"||op=="seed"){
   const auto r=op=="roll"?roller.roll(step.at("value")):roller.rollWithSeed(step.at("value"));
   out.push_back(Json{{"bones",r->bones.snapshot()},{"inspirationSeed",r->inspirationSeed}});
  }else if(op=="hash")out.push_back(monster::hashString(step.at("value"),s.bunHash));
  else if(op=="random"){
   auto rng=monster::mulberry32(step.at("value").get<std::uint32_t>());Json values=Json::array();
   for(int i=0;i<step.at("count").get<int>();++i)values.push_back(rng());out.push_back(values);
  }else if(op=="monsterAlias"){
   auto m=roller.getMonster();const auto r=roller.roll(roller.monsterUserId());
   (*m->stats)["CHAOS"]=777;out.push_back(Json::array({m->stats==r->bones.stats,(*r->bones.stats)["CHAOS"]}));
  }else if(op=="user")out.push_back(roller.monsterUserId());
  else if(op=="get"){const auto m=roller.getMonster();out.push_back(m?m->snapshot():Json());}
  else if(op=="identity")out.push_back(roller.roll(step.at("value"))==roller.roll(step.at("value")));
  else if(op=="cacheMutation"){
   roller.roll(step.at("value"))->bones.stats.operator*()["CHAOS"]=999;
   out.push_back(roller.roll(step.at("value"))->bones.stats.operator*()["CHAOS"]);
  }
 }
 return out;
}
int main(){const auto input=value::parse(std::string(std::istreambuf_iterator<char>{std::cin},{}));Json out=Json::array();for(const auto& item:input)out.push_back(run(item));std::cout<<value::stringify(out)<<'\n';}
