#include "keyboard_input/Keyboard.hpp"
#include "keyboard_input/Regex.hpp"
namespace batbox::keyboard_input {
const glob::Regex16& sgrMouseRegex(){static const glob::Regex16 re(u"^\x1b\\[<(\\d+);(\\d+);(\\d+)([Mm])$");return re;}
std::vector<double> splitNumericParams(const std::string& params){
  std::vector<double> result;if(params.empty())return result;
  std::size_t start=0;
  for(;;){const auto end=params.find(';',start);result.push_back(value::parseInt10(params.substr(start,end==std::string::npos?end:end-start)));if(end==std::string::npos)break;start=end+1;}
  return result;
}
std::optional<Json> parseTerminalResponse(const std::string& s){
  Match m;
  if(s.starts_with("\x1b[")){
    static const glob::Regex16 decrpm(u"^\x1b\\[\\?(\\d+);(\\d+)\\$y$");
    static const glob::Regex16 da1(u"^\x1b\\[\\?([\\d;]*)c$");
    static const glob::Regex16 da2(u"^\x1b\\[>([\\d;]*)c$");
    static const glob::Regex16 kitty(u"^\x1b\\[\\?(\\d+)u$");
    static const glob::Regex16 cursor(u"^\x1b\\[\\?(\\d+);(\\d+)R$");
    if(match(decrpm,s,m))return Json{{"type","decrpm"},{"mode",value::parseInt10(group(m,1))},{"status",value::parseInt10(group(m,2))}};
    if(match(da1,s,m))return Json{{"type","da1"},{"params",splitNumericParams(group(m,1))}};
    if(match(da2,s,m))return Json{{"type","da2"},{"params",splitNumericParams(group(m,1))}};
    if(match(kitty,s,m))return Json{{"type","kittyKeyboard"},{"flags",value::parseInt10(group(m,1))}};
    if(match(cursor,s,m))return Json{{"type","cursorPosition"},{"row",value::parseInt10(group(m,1))},{"col",value::parseInt10(group(m,2))}};
    return {};
  }
  if(s.starts_with("\x1b]")){
    static const glob::Regex16 osc(u"(?s)^\x1b\\](\\d+);(.*?)(?:\x07|\x1b\\\\)$");
    if(match(osc,s,m))return Json{{"type","osc"},{"code",value::parseInt10(group(m,1))},{"data",group(m,2)}};
  }
  if(s.starts_with("\x1bP")){
    static const glob::Regex16 xt(u"(?s)^\x1bP>\\|(.*?)(?:\x07|\x1b\\\\)$");
    if(match(xt,s,m))return Json{{"type","xtversion"},{"name",group(m,1)}};
    // DECRQSS replies (for example the cursor-shape query FTXUI sends at
    // startup) and any other complete device control string are terminal
    // answers, never typed text.
    static const glob::Regex16 decrqss(u"(?s)^\x1bP([01])\\$r(.*?)(?:\x07|\x1b\\\\)$");
    if(match(decrqss,s,m))return Json{{"type","decrqss"},
      {"valid",group(m,1)=="1"},{"data",group(m,2)}};
    static const glob::Regex16 dcs(u"(?s)^\x1bP(.*?)(?:\x07|\x1b\\\\)$");
    if(match(dcs,s,m))return Json{{"type","dcs"},{"data",group(m,1)}};
  }
  return {};
}
std::optional<ParsedMouse> parseMouseEvent(const std::string& s){
  Match m;if(!match(sgrMouseRegex(),s,m))return {};
  const auto button=value::parseInt10(group(m,1));if(toUint32(button)&0x40)return {};
  return ParsedMouse{button,value::parseInt10(group(m,2)),value::parseInt10(group(m,3)),group(m,4)=="M"?"press":"release",s};
}
}
