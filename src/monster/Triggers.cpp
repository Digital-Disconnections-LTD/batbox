#include "monster/Poetry.hpp"
namespace batbox::monster {
bool isMonsterTeaserWindow(LocalDate d){return d.year==2026&&d.month==3&&d.day<=7;}
bool isMonsterLive(LocalDate d){return d.year>2026||(d.year==2026&&d.month>=3);}
Json findMonsterTriggerPositions(const std::string& text,bool enabled){
  Json triggers=Json::array();if(!enabled)return triggers;
  const auto units=value::utf16CodeUnits(text);const std::u16string command=u"/monster";
  std::size_t offset=0;
  while((offset=units.find(command,offset))!=std::u16string::npos){
    const auto end=offset+command.size();
    const bool nextWord=end<units.size()&&((units[end]>=u'a'&&units[end]<=u'z')||
      (units[end]>=u'A'&&units[end]<=u'Z')||(units[end]>=u'0'&&units[end]<=u'9')||units[end]==u'_');
    if(!nextWord)triggers.push_back(Json{{"start",offset},{"end",end}});
    offset=end;
  }
  return triggers;
}
}
