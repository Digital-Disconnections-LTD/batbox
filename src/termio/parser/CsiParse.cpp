#include "termio/parser/Parser.hpp"
#include "termio/parser/CsiData.hpp"
#include <cmath>

namespace batbox::termio::parser {
namespace {
std::vector<double> parseCSIParams(const std::string& params){
  if(params.empty())return {};
  std::vector<double> result;std::size_t start=0;
  while(start<=params.size()){
    const auto end=params.find_first_of(";:",start);
    const auto piece=params.substr(start,end==std::string::npos?end:end-start);
    result.push_back(piece.empty()?0:value::parseInt10(piece));
    if(end==std::string::npos)break;
    start=end+1;
  }
  return result;
}
CursorAction move(const std::string& direction,double count){
  CursorAction action;action.type="move";action.direction=direction;action.count=count;return action;
}
CursorAction cursorCount(const std::string& type,double count){
  CursorAction action;action.type=type;action.count=count;return action;
}
ScrollAction scrollCount(const std::string& type,double count){
  ScrollAction action;action.type=type;action.count=count;return action;
}
}
std::optional<Action> parseCSI(const std::string& rawSequence){
  const auto units=value::utf16CodeUnits(rawSequence);
  if(units.size()<=2)return std::nullopt;
  const auto inner=std::u16string_view(units).substr(2);
  const auto finalByte=inner.back();
  const auto beforeFinal=inner.substr(0,inner.size()-1);
  std::u16string privateMode,paramStr(beforeFinal),intermediate;
  if(!beforeFinal.empty()&&(beforeFinal[0]==u'?'||beforeFinal[0]==u'>'||beforeFinal[0]==u'=')){
    privateMode=beforeFinal.substr(0,1);paramStr=std::u16string(beforeFinal.substr(1));
  }
  std::size_t suffix=paramStr.size();
  while(suffix>0){
    const auto c=paramStr[suffix-1];
    if((c>=u'0'&&c<=u'9')||c==u';'||c==u':')break;
    --suffix;
  }
  if(suffix<paramStr.size()){
    intermediate=paramStr.substr(suffix);paramStr.resize(suffix);
  }
  const auto params=parseCSIParams(value::fromUtf16(paramStr));
  const double p0=params.empty()?1:params[0];
  const double p1=params.size()<2?1:params[1];
  if(finalByte==u'm'&&privateMode.empty())return SgrAction{value::fromUtf16(paramStr)};

  if(finalByte==u'A')return move("up",p0);
  if(finalByte==u'B')return move("down",p0);
  if(finalByte==u'C')return move("forward",p0);
  if(finalByte==u'D')return move("back",p0);
  if(finalByte==u'E')return cursorCount("nextLine",p0);
  if(finalByte==u'F')return cursorCount("prevLine",p0);
  if(finalByte==u'G'){
    CursorAction action;action.type="column";action.col=p0;return action;
  }
  if(finalByte==u'H'||finalByte==u'f'){
    CursorAction action;action.type="position";action.row=p0;action.col=p1;return action;
  }
  if(finalByte==u'd'){
    CursorAction action;action.type="row";action.row=p0;return action;
  }
  if(finalByte==u'J'){
    return EraseAction{"display",data::eraseDisplay(params.empty()?0:params[0]),1};
  }
  if(finalByte==u'K'){
    return EraseAction{"line",data::eraseLine(params.empty()?0:params[0]),1};
  }
  if(finalByte==u'X')return EraseAction{"chars","",p0};
  if(finalByte==u'S')return scrollCount("up",p0);
  if(finalByte==u'T')return scrollCount("down",p0);
  if(finalByte==u'r'){
    ScrollAction action;action.type="setRegion";action.top=p0;action.bottom=p1;return action;
  }
  if(finalByte==u's'){
    CursorAction action;action.type="save";return action;
  }
  if(finalByte==u'u'){
    CursorAction action;action.type="restore";return action;
  }
  if(finalByte==u'q'&&intermediate==u" "){
    CursorAction action;action.type="style";
    const auto [style,blinking]=data::cursorStyle(p0);
    action.style=style;action.blinking=blinking;return action;
  }
  if(privateMode==u"?"&&(finalByte==u'h'||finalByte==u'l')){
    const bool enabled=finalByte==u'h';
    if(p0==data::CURSOR_VISIBLE){
      CursorAction action;action.type=enabled?"show":"hide";return action;
    }
    if(p0==data::ALT_SCREEN_CLEAR||p0==data::ALT_SCREEN)
      return ModeAction{"alternateScreen","",enabled};
    if(p0==data::BRACKETED_PASTE)return ModeAction{"bracketedPaste","",enabled};
    if(p0==data::MOUSE_NORMAL)return ModeAction{"mouseTracking",enabled?"normal":"off",false};
    if(p0==data::MOUSE_BUTTON)return ModeAction{"mouseTracking",enabled?"button":"off",false};
    if(p0==data::MOUSE_ANY)return ModeAction{"mouseTracking",enabled?"any":"off",false};
    if(p0==data::FOCUS_EVENTS)return ModeAction{"focusEvents","",enabled};
  }
  return UnknownAction{rawSequence};
}
std::string identifySequence(const std::string& sequence){
  const auto units=value::utf16CodeUnits(sequence);
  if(units.size()<2||units[0]!=0x1b)return "unknown";
  if(units[1]==0x5b)return "csi";
  if(units[1]==0x5d)return "osc";
  if(units[1]==0x4f)return "ss3";
  return "esc";
}
}
