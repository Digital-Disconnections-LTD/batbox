#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::monster {
std::string monsterIntroText(const std::string& name,const std::string& species);
struct IntroServices {
  std::function<bool(const std::string&)> feature;
  std::function<std::optional<Json>()> getMonster;
  std::function<Json()> getGlobalConfig;
};
Json getMonsterIntroAttachment(const std::optional<Json>& messages,const IntroServices& services);
struct LocalDate {int year,month,day;}; // month follows JavaScript's zero-based index
bool isMonsterTeaserWindow(LocalDate date);
bool isMonsterLive(LocalDate date);
Json findMonsterTriggerPositions(const std::string& text,bool enabled);
}
