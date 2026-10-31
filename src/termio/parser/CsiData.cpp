#include "termio/parser/CsiData.hpp"
namespace batbox::termio::parser::data {
std::string eraseDisplay(double index){
  if(index==1)return "toStart";
  if(index==2)return "all";
  if(index==3)return "scrollback";
  return "toEnd";
}
std::string eraseLine(double index){
  if(index==1)return "toStart";
  if(index==2)return "all";
  return "toEnd";
}
std::pair<std::string,bool> cursorStyle(double index){
  if(index==2)return {"block",false};
  if(index==3)return {"underline",true};
  if(index==4)return {"underline",false};
  if(index==5)return {"bar",true};
  if(index==6)return {"bar",false};
  return {"block",true};
}
}
