#include "terminal_color/ChalkInternal.hpp"
#include <array>
#include <stdexcept>

namespace batbox::terminal_color {
namespace {
struct Code {const char* name;int open,close;};
constexpr std::array<Code,22> named{{
  {"bold",1,22},{"dim",2,22},{"italic",3,23},
  {"underline",4,24},{"inverse",7,27},{"strikethrough",9,29},
  {"black",30,39},{"red",31,39},{"green",32,39},
  {"yellow",33,39},{"blue",34,39},{"magenta",35,39},
  {"cyan",36,39},{"white",37,39},
  {"blackBright",90,39},{"redBright",91,39},
  {"greenBright",92,39},{"yellowBright",93,39},
  {"blueBright",94,39},{"magentaBright",95,39},
  {"cyanBright",96,39},{"whiteBright",97,39}
}};
std::string replaceClosers(std::string value,const std::string& close,
    const std::string& open){
  std::size_t position=0;
  while((position=value.find(close,position))!=std::string::npos){
    value.insert(position+close.size(),open);
    position+=close.size()+open.size();
  }
  return value;
}
std::string encaseLinefeeds(const std::string& value,
    const std::string& close,const std::string& open){
  std::string result;std::size_t begin=0;
  for(std::size_t i=0;i<value.size();i++){
    if(value[i]!='\n')continue;
    const bool cr=i>0&&value[i-1]=='\r';
    result+=value.substr(begin,(cr?i-1:i)-begin)+close;
    result+=cr?"\r\n":"\n";
    result+=open;begin=i+1;
  }
  result+=value.substr(begin);
  return result;
}
}
std::string chalkWrap(const std::string& text,const std::string& open,
    const std::string& close,double level){
  if(level<=0||text.empty())return text;
  auto value=text;
  if(value.find('\x1b')!=std::string::npos)
    value=replaceClosers(std::move(value),close,open);
  if(value.find('\n')!=std::string::npos)
    value=encaseLinefeeds(value,close,open);
  return open+value+close;
}
std::string chalkNamed(const std::string& text,
    const std::string& name,double level){
  for(const auto& code:named){
    if(name==code.name){
      return chalkWrap(text,"\x1b["+std::to_string(code.open)+"m",
        "\x1b["+std::to_string(code.close)+"m",level);
    }
  }
  if(name.size()>2&&name.starts_with("bg")){
    const auto foreground=name.substr(2);
    if(!foreground.empty()){
      auto lower=foreground;lower[0]=static_cast<char>(
        lower[0]>='A'&&lower[0]<='Z'?lower[0]+32:lower[0]);
      for(const auto& code:named){
        if(lower==code.name&&code.open>=30){
          return chalkWrap(text,"\x1b["+std::to_string(code.open+10)+"m",
            "\x1b[49m",level);
        }
      }
    }
  }
  throw std::invalid_argument("Unknown Chalk style: "+name);
}
} // namespace batbox::terminal_color
