#include "termio/Csi.hpp"
#include "runtime/ArrayJoin.hpp"
#include <stdexcept>
namespace batbox::termio {
bool isCSIParam(double byte){return byte>=0x30&&byte<=0x3f;}
bool isCSIIntermediate(double byte){return byte>=0x20&&byte<=0x2f;}
bool isCSIFinal(double byte){return byte>=0x40&&byte<=0x7e;}
namespace {
std::string argument(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_number())return value::numberToString(value.get<double>());
  throw std::invalid_argument("CSI argument requires a string or number");
}
std::string withCount(double n,const std::string& command){
  // A numeric NaN or Infinity is still formatted by JavaScript interpolation.
  return CSI_PREFIX+value::numberToString(n)+command;
}
}
std::string csi(const Json& args){
  if(args.empty())return CSI_PREFIX;
  if(args.size()==1)return CSI_PREFIX+argument(args.at(0));
  Json params=Json::array();
  for(std::size_t i=0;i+1<args.size();++i)params.push_back(args.at(i));
  return CSI_PREFIX+value::arrayJoin(params,";")+argument(args.back());
}
std::string cursorUp(double n){return n==0?"":withCount(n,"A");}
std::string cursorDown(double n){return n==0?"":withCount(n,"B");}
std::string cursorForward(double n){return n==0?"":withCount(n,"C");}
std::string cursorBack(double n){return n==0?"":withCount(n,"D");}
std::string cursorTo(double col){return withCount(col,"G");}
std::string cursorPosition(double row,double col){
  return CSI_PREFIX+value::numberToString(row)+";"+value::numberToString(col)+"H";
}
std::string cursorMove(double x,double y){
  std::string result;
  if(x<0)result+=cursorBack(-x);else if(x>0)result+=cursorForward(x);
  if(y<0)result+=cursorUp(-y);else if(y>0)result+=cursorDown(y);
  return result;
}
std::string eraseToEndOfLine(){return csi(Json::array({"K"}));}
std::string eraseToStartOfLine(){return csi(Json::array({1,"K"}));}
std::string eraseLine(){return csi(Json::array({2,"K"}));}
std::string eraseToEndOfScreen(){return csi(Json::array({"J"}));}
std::string eraseToStartOfScreen(){return csi(Json::array({1,"J"}));}
std::string eraseScreen(){return csi(Json::array({2,"J"}));}
std::string eraseLines(double n){
  if(n<=0)return "";
  std::string result;
  for(double i=0;i<n;++i){result+=ERASE_LINE;if(i<n-1)result+=cursorUp(1);}
  result+=CURSOR_LEFT;return result;
}
std::string scrollUp(double n){return n==0?"":withCount(n,"S");}
std::string scrollDown(double n){return n==0?"":withCount(n,"T");}
std::string setScrollRegion(double top,double bottom){
  return CSI_PREFIX+value::numberToString(top)+";"+value::numberToString(bottom)+"r";
}
}
