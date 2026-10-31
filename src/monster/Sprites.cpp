#include "monster/Sprites.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace batbox::monster {
std::vector<std::string> renderSprite(const Json& bones,double frame){
  const auto& frames=spriteData().bodies.at(bones.at("species").get<std::string>());
  const auto index=std::fmod(frame,static_cast<double>(frames.size()));
  if(!std::isfinite(index)||index<0||std::floor(index)!=index)
    throw std::invalid_argument("Sprite frame must select an existing frame");
  const auto& body=frames.at(static_cast<std::size_t>(index));
  std::vector<std::string> lines;const auto eye=bones.at("eye").get<std::string>();
  for(const auto& entry:body){auto line=entry.get<std::string>();std::size_t offset=0;
    while((offset=line.find("{E}",offset))!=std::string::npos){line.replace(offset,3,eye);offset+=eye.size();}
    lines.push_back(std::move(line));
  }
  const auto hat=bones.at("hat").get<std::string>();
  if(hat!="none"&&value::trim(lines.at(0)).empty())lines[0]=spriteData().hats.at(hat);
  if(value::trim(lines.at(0)).empty()&&std::all_of(frames.begin(),frames.end(),
      [](const Json& f){return value::trim(f.at(0).get<std::string>()).empty();}))lines.erase(lines.begin());
  return lines;
}
std::size_t spriteFrameCount(const std::string& species){return spriteData().bodies.at(species).size();}
std::optional<std::string> renderFace(const Json& bones){
  const auto eye=bones.at("eye").get<std::string>(),species=bones.at("species").get<std::string>();
  if(species=="bat"||species=="gargoyle")return "("+eye+">";
  if(species=="ooze")return "("+eye+eye+")";
  if(species=="imp")return "="+eye+"ω"+eye+"=";
  if(species=="dragon")return "<"+eye+"~"+eye+">";
  if(species=="kraken")return "~("+eye+eye+")~";
  if(species=="cyclops")return "("+eye+")("+eye+")";
  if(species=="gremlin")return "("+eye+">)";
  if(species=="troll")return "["+eye+"_"+eye+"]";
  if(species=="wurm")return eye+"(@)";
  if(species=="ghost")return "/"+eye+eye+"\\";
  if(species=="aberration")return "}"+eye+"."+eye+"{";
  if(species=="ogre")return "("+eye+"oo"+eye+")";
  if(species=="mandrake")return "|"+eye+"  "+eye+"|";
  if(species=="golem")return "["+eye+eye+"]";
  if(species=="jackalope")return "("+eye+".."+eye+")";
  if(species=="myconid")return "|"+eye+"  "+eye+"|";
  if(species=="behemoth")return "("+eye+"."+eye+")";
  return {};
}
} // namespace batbox::monster
