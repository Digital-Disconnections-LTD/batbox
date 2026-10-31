#include "shell/ShellQuote.hpp"
#include <stdexcept>

namespace batbox::shell_quote {
namespace {
bool whitespace(char16_t unit){
  const auto text=value::fromUtf16(std::u16string_view(&unit,1));
  return value::trim(text).empty();
}
bool meta(char16_t c){
  static const std::u16string symbols=u"#!\"$&'()*,:;<=>?@[\\]^`{|}";
  return symbols.find(c)!=std::u16string::npos;
}
bool asciiLetter(char16_t c){return (c>=u'A'&&c<=u'Z')||(c>=u'a'&&c<=u'z');}
}
std::string quote(const std::vector<Json>& arguments){
  std::string result;bool first=true;
  for(const auto& argument:arguments){
    if(!first)result+=' ';
    first=false;
    if(argument.is_object()){
      if(!argument.contains("op")||!argument["op"].is_string())
        throw std::invalid_argument("shell-quote object token has no op string");
      const auto units=value::utf16CodeUnits(argument["op"].get<std::string>());
      for(char16_t c:units){
        if(c!=u'\n'&&c!=u'\r'&&c!=u'\u2028'&&c!=u'\u2029')result+='\\';
        result+=value::fromUtf16(std::u16string_view(&c,1));
      }
      continue;
    }
    std::string source=argument.is_string()?argument.get<std::string>():
      argument.is_null()?"null":argument.is_boolean()?(argument.get<bool>()?"true":"false"):
      argument.is_number()?value::numberToString(argument.get<double>()):value::stringify(argument);
    if(source.empty()){result+="''";continue;}
    const auto units=value::utf16CodeUnits(source);
    bool quoteTrigger=false,single=false,doubleTrigger=false;
    for(char16_t c:units){
      quoteTrigger|=c==u'"'||c==u'\\'||whitespace(c);
      single|=c==u'\'';
      doubleTrigger|=c==u'"'||c==u'\''||whitespace(c);
    }
    if(quoteTrigger&&!single){result+='\'';result+=source;result+='\'';continue;}
    if(doubleTrigger){
      result+='"';
      for(char16_t c:units){
        if(c==u'"'||c==u'\\'||c==u'$'||c==u'`'||c==u'!')result+='\\';
        result+=value::fromUtf16(std::u16string_view(&c,1));
      }
      result+='"';continue;
    }
    for(std::size_t i=0;i<units.size();){
      if(i+2<units.size()&&asciiLetter(units[i])&&units[i+1]==u':'&&meta(units[i+2])){
        result+=value::fromUtf16(std::u16string_view(units.data()+i,2));
        result+='\\';result+=value::fromUtf16(std::u16string_view(&units[i+2],1));i+=3;
      }else{
        if(meta(units[i]))result+='\\';
        result+=value::fromUtf16(std::u16string_view(&units[i],1));++i;
      }
    }
  }
  return result;
}
} // namespace batbox::shell_quote
