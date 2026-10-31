#include "monster/Monster.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::monster {
namespace {
std::shared_ptr<Roll> rollFrom(Random rng){
  const auto& data=tables();const auto rarity=rollRarity(rng);
  const auto pick=[&](const Json& values){return values.at(static_cast<std::size_t>(
    std::floor(rng()*static_cast<double>(values.size()))));};
  Bones bones;
  bones.rarity=rarity;bones.species=pick(data.species).get<std::string>();
  bones.eye=pick(data.eyes).get<std::string>();
  bones.hat=rarity=="common"?"none":pick(data.hats).get<std::string>();
  bones.shiny=rng()<0.01;bones.stats=std::make_shared<Json>(rollStats(rng,rarity));
  return std::make_shared<Roll>(Roll{std::move(bones),std::floor(rng()*1e9)});
}
}
Json Bones::snapshot()const{
  return Json{{"rarity",rarity},{"species",species},{"eye",eye},{"hat",hat},
    {"shiny",shiny},{"stats",*stats}};
}
Json Monster::snapshot()const{auto result=fields;result["stats"]=*stats;return result;}
Roller::Roller(Services services):services_(std::move(services)){}
std::shared_ptr<Roll> Roller::roll(const std::string& userId){
  const auto key=userId+"friend-2026-401";
  if(stashKey_&&*stashKey_==key)return stashValue_;
  const auto value=rollFrom(mulberry32(hashString(key,services_.bunHash)));
  stashKey_=key;stashValue_=value;return value;
}
std::shared_ptr<Roll> Roller::rollWithSeed(const std::string& seed)const{
  return rollFrom(mulberry32(hashString(seed,services_.bunHash)));
}
std::string Roller::monsterUserId()const{
  const auto config=services_.getGlobalConfig();
  const auto& account=value::property(config,"oauthAccount");
  const auto& uuid=value::property(account,"accountUuid");
  if(!uuid.is_null())return uuid.get<std::string>();
  const auto& user=value::property(config,"userID");
  return user.is_null()?"anon":user.get<std::string>();
}
std::optional<Monster> Roller::getMonster(){
  const auto stored=value::property(services_.getGlobalConfig(),"monster");
  if(!value::truthy(stored))return {};
  const auto bones=roll(monsterUserId())->bones;
  const auto values=bones.snapshot();
  Json result=stored;
  if(!result.is_object())throw std::invalid_argument("Stored monster must be a configuration object");
  for(const auto& key:value::objectKeys(values))result[key]=values[key];
  result.erase("stats");
  return Monster{std::move(result),bones.stats};
}
} // namespace batbox::monster
