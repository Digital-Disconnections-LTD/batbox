#include "monster/Sprites.hpp"
#include <iostream>
#include <iterator>
using namespace batbox;
Json run(const Json& item){
 Json calls=Json::array();monster::GeometryServices s;
 s.feature=[&](const std::string& key){calls.push_back(Json::array({"feature",key}));return item["enabled"].get<bool>();};
 s.getMonster=[&]()->std::optional<Json>{calls.push_back(Json::array({"getMonster"}));if(item["monster"].is_null())return {};return item["monster"];};
 s.getGlobalConfig=[&]{calls.push_back(Json::array({"config"}));return item.value("config",Json::object());};
 s.stringWidth=[&](const std::string& name){calls.push_back(Json::array({"width",name}));return item["nameWidth"].get<double>();};
 s.isFullscreenActive=[&]{calls.push_back(Json::array({"fullscreen"}));return item["fullscreen"].get<bool>();};
 const auto columns=monster::monsterReservedColumns(item["columns"],item["speaking"],s);
 const auto face=monster::renderFace(item["bones"]);
 return Json{{"lines",monster::renderSprite(item["bones"],item["frame"])},
  {"count",monster::spriteFrameCount(item["bones"]["species"])},{"face",face?Json(*face):Json()},
  {"wrapped",monster::wrap(item["text"],item["width"])},{"columns",columns},{"calls",calls}};
}
int main(){const auto input=value::parse(std::string(std::istreambuf_iterator<char>{std::cin},{}));Json out=Json::array();for(const auto& item:input)out.push_back(run(item));std::cout<<value::stringify(out)<<'\n';}
