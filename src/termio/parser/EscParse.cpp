#include "termio/parser/Parser.hpp"

namespace batbox::termio::parser {
std::optional<Action> parseEsc(const std::string& chars){
  const auto units=value::utf16CodeUnits(chars);
  if(units.empty())return std::nullopt;
  const auto first=units[0];
  if(first==u'c')return ResetAction{};
  if(first==u'7'){
    CursorAction action;action.type="save";return action;
  }
  if(first==u'8'){
    CursorAction action;action.type="restore";return action;
  }
  if(first==u'D'){
    CursorAction action;action.type="move";action.direction="down";return action;
  }
  if(first==u'M'){
    CursorAction action;action.type="move";action.direction="up";return action;
  }
  if(first==u'E'){
    CursorAction action;action.type="nextLine";return action;
  }
  if(first==u'H')return std::nullopt;
  if((first==u'('||first==u')')&&units.size()>=2)return std::nullopt;
  return UnknownAction{"\x1b"+chars};
}
}
