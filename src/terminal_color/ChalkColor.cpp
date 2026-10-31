#include "terminal_color/ChalkInternal.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <regex>

namespace batbox::terminal_color {
namespace {
double roundJs(double x){return std::floor(x+0.5);}
double rgbToAnsi256(double red,double green,double blue){
  if(red==green&&green==blue){
    if(red<8)return 16;
    if(red>248)return 231;
    return roundJs(((red-8)/247)*24)+232;
  }
  return 16+36*roundJs(red/255*5)+6*roundJs(green/255*5)+
    roundJs(blue/255*5);
}
double ansi256ToAnsi(double code){
  if(code<8)return 30+code;
  if(code<16)return 90+(code-8);
  double red,green,blue;
  if(code>=232){red=(((code-232)*10)+8)/255;green=red;blue=red;}
  else{
    code-=16;
    const auto remainder=std::fmod(code,36);
    red=std::floor(code/36)/5;
    green=std::floor(remainder/6)/5;
    blue=std::fmod(remainder,6)/5;
  }
  const auto value=std::max({red,green,blue})*2;
  if(value==0)return 30;
  auto result=30+static_cast<double>(
    (static_cast<std::int32_t>(roundJs(blue))<<2)|
    (static_cast<std::int32_t>(roundJs(green))<<1)|
    static_cast<std::int32_t>(roundJs(red)));
  if(value==2)result+=60;
  return result;
}
std::string sgr(double code){return "\x1b["+value::numberToString(code)+"m";}
std::string rgbSequence(double red,double green,double blue,
    bool background,double level){
  if(level==3)return "\x1b["+std::to_string(background?48:38)+
    ";2;"+value::numberToString(red)+";"+value::numberToString(green)+
    ";"+value::numberToString(blue)+"m";
  const auto code=rgbToAnsi256(red,green,blue);
  if(level==2)return "\x1b["+std::to_string(background?48:38)+
    ";5;"+value::numberToString(code)+"m";
  return sgr(ansi256ToAnsi(code)+(background?10:0));
}
std::array<double,3> hexToRgb(const std::string& hex){
  static const std::regex pattern("[a-fA-F0-9]{6}|[a-fA-F0-9]{3}");
  std::smatch match;
  if(!std::regex_search(hex,match,pattern))return {0,0,0};
  auto digits=match.str();
  if(digits.size()==3){
    digits={digits[0],digits[0],digits[1],digits[1],digits[2],digits[2]};
  }
  const auto value=std::stoul(digits,nullptr,16);
  return {static_cast<double>((value>>16)&255),
    static_cast<double>((value>>8)&255),
    static_cast<double>(value&255)};
}
}
ChalkState initializeChalkState(double detectedLevel,
    const std::optional<std::string>& termProgram,
    const std::optional<std::string>& tmux,bool tmuxTruecolor){
  ChalkState state;state.level=detectedLevel;
  if(termProgram=="vscode"&&state.level==2){
    state.level=3;state.boostedForXtermJs=true;
  }
  if(!tmuxTruecolor&&tmux&&!tmux->empty()&&state.level>2){
    state.level=2;state.clampedForTmux=true;
  }
  return state;
}
std::string chalkAnsi256(const std::string& text,double code,
    bool background,double level){
  return chalkWrap(text,"\x1b["+std::to_string(background?48:38)+
    ";5;"+value::numberToString(code)+"m",
    background?"\x1b[49m":"\x1b[39m",level);
}
std::string chalkRgb(const std::string& text,double red,double green,
    double blue,bool background,double level){
  return chalkWrap(text,rgbSequence(red,green,blue,background,level),
    background?"\x1b[49m":"\x1b[39m",level);
}
std::string chalkHex(const std::string& text,const std::string& hex,
    bool background,double level){
  const auto rgb=hexToRgb(hex);
  return chalkRgb(text,rgb[0],rgb[1],rgb[2],background,level);
}
} // namespace batbox::terminal_color
