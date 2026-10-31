#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::monster {
struct SpriteData {Json bodies,hats;};
const SpriteData& spriteData();
std::vector<std::string> renderSprite(const Json& bones,double frame=0);
std::size_t spriteFrameCount(const std::string& species);
std::optional<std::string> renderFace(const Json& bones);
std::vector<std::string> wrap(const std::string& text,double width);
struct GeometryServices {
  std::function<bool(const std::string&)> feature;
  std::function<std::optional<Json>()> getMonster;
  std::function<Json()> getGlobalConfig;
  std::function<double(const std::string&)> stringWidth;
  std::function<bool()> isFullscreenActive;
};
double monsterReservedColumns(double terminalColumns,bool speaking,const GeometryServices& services);
}
