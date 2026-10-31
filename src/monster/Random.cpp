#include "monster/Monster.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::monster {
Random mulberry32(std::uint32_t seed){
  return [a=seed]()mutable{
    a+=0x6d2b79f5u;
    std::uint32_t t=(a^(a>>15u))*(1u|a);
    t=(t+(t^(t>>7u))*(61u|t))^t;
    return static_cast<double>(t^(t>>14u))/4294967296.0;
  };
}
std::uint32_t hashString(const std::string& text,
    const std::function<std::uint64_t(const std::string&)>& bunHash){
  if(bunHash)return static_cast<std::uint32_t>(bunHash(text)&0xffffffffull);
  std::uint32_t h=2166136261u;
  for(const auto unit:value::utf16CodeUnits(text)){h^=unit;h*=16777619u;}
  return h;
}
namespace {
std::string pick(Random& rng,const Json& values){
  const auto index=static_cast<std::size_t>(std::floor(rng()*static_cast<double>(values.size())));
  return values.at(index).get<std::string>();
}
}
std::string rollRarity(Random& rng){
  const auto& data=tables();double total=0;
  for(const auto& weight:data.weights)total+=weight.get<double>();
  double value=rng()*total;
  for(const auto& rarity:data.rarities){
    const auto name=rarity.get<std::string>();value-=data.weights.at(name).get<double>();
    if(value<0)return name;
  }
  return "common";
}
Json rollStats(Random& rng,const std::string& rarity){
  const auto& data=tables();const auto floor=data.floors.at(rarity).get<double>();
  const auto peak=pick(rng,data.statNames);auto dump=pick(rng,data.statNames);
  while(dump==peak)dump=pick(rng,data.statNames);
  Json stats=Json::object();
  for(const auto& entry:data.statNames){const auto name=entry.get<std::string>();
    if(name==peak)stats[name]=std::min(100.0,floor+50+std::floor(rng()*30));
    else if(name==dump)stats[name]=std::max(1.0,floor-10+std::floor(rng()*15));
    else stats[name]=floor+std::floor(rng()*40);
  }
  return stats;
}
} // namespace batbox::monster
