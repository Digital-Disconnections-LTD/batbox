#pragma once
#include "runtime/JsonValue.hpp"
#include <string>
#include <vector>

namespace batbox::monster {
struct SpriteData {Json bodies,hats;};
const SpriteData& spriteData();
std::vector<std::string> renderSprite(const Json& bones,double frame=0);
}
