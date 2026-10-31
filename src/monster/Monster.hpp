#pragma once
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::monster {
struct Tables {
  Json rarities,species,eyes,hats,statNames,weights,floors,stars,colors;
};
const Tables& tables();
using Random=std::function<double()>;
Random mulberry32(std::uint32_t seed);
std::uint32_t hashString(const std::string& text,
  const std::function<std::uint64_t(const std::string&)>& bunHash={});
std::string rollRarity(Random& rng);
Json rollStats(Random& rng,const std::string& rarity);
struct Bones {
  std::string rarity,species,eye,hat;
  bool shiny=false;
  std::shared_ptr<Json> stats;
  Json snapshot()const;
};
struct Roll {Bones bones;double inspirationSeed=0;};
struct Monster {
  Json fields;
  std::shared_ptr<Json> stats;
  Json snapshot()const;
};
struct Services {
  std::function<Json()> getGlobalConfig;
  std::function<std::uint64_t(const std::string&)> bunHash;
};
class Roller {
public:
  explicit Roller(Services services);
  std::shared_ptr<Roll> roll(const std::string& userId);
  std::shared_ptr<Roll> rollWithSeed(const std::string& seed)const;
  std::string monsterUserId()const;
  std::optional<Monster> getMonster();
private:
  Services services_;
  std::optional<std::string> stashKey_;
  std::shared_ptr<Roll> stashValue_;
};
} // namespace batbox::monster
