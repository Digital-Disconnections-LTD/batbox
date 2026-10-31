#include "monster/Sprites.hpp"
#include <algorithm>
#include <cmath>
namespace batbox::monster {
std::vector<std::string> wrap(const std::string& text,double width){
  std::vector<std::string> lines;std::string cur;std::size_t start=0;
  for(;;){const auto end=text.find(' ',start);const auto word=text.substr(start,end==std::string::npos?end:end-start);
    if(static_cast<double>(value::stringLength(cur)+value::stringLength(word)+1)>width&&!cur.empty()){
      lines.push_back(cur);cur=word;
    }else cur=cur.empty()?word:cur+" "+word;
    if(end==std::string::npos)break;start=end+1;
  }
  if(!cur.empty())lines.push_back(cur);return lines;
}
namespace {
double spriteColWidth(double nameWidth){return std::isnan(nameWidth)?nameWidth:std::max(12.0,nameWidth+2);}
}
double monsterReservedColumns(double columns,bool speaking,const GeometryServices& s){
  if(!s.feature("MONSTER"))return 0;
  const auto monster=s.getMonster();
  if(!monster||value::truthy(value::property(s.getGlobalConfig(),"monsterMuted")))return 0;
  if(columns<100)return 0;
  const auto nameWidth=s.stringWidth(monster->at("name"));
  const auto bubble=speaking&&!s.isFullscreenActive()?36:0;
  return spriteColWidth(nameWidth)+2+bubble;
}
} // namespace batbox::monster
