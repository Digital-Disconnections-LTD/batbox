#include "shell/ShellQuote.hpp"
#include "runtime/Platform.hpp"
#include <regex>
#include <stdexcept>
#include <utility>

namespace batbox::shell_quote {
namespace {
std::wstring widen(const std::string& ascii){
  std::wstring result;result.reserve(ascii.size());
  for(unsigned char c:ascii)result+=static_cast<wchar_t>(c);
  return result;
}
std::wstring regexInput(const std::u16string& input){
  std::wstring result;result.reserve(input.size());
  for(char16_t unit:input){
    const auto text=value::fromUtf16(std::u16string_view(&unit,1));
    const bool space=value::trim(text).empty();
    result+=static_cast<wchar_t>(space&&unit>0x7f?u' ':unit);
  }
  return result;
}
std::string fromUnits(const std::u16string& units){
  return value::fromUtf16(std::u16string_view(units.data(),units.size()));
}
std::string jsPrimitive(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  return value::stringify(value);
}
bool variableSpecial(char16_t c){
  static const std::u16string special=u"*@#?$!_-";
  return special.find(c)!=std::u16string::npos;
}
bool variableWord(char16_t c){
  return (c>=u'A'&&c<=u'Z')||(c>=u'a'&&c<=u'z')||
    (c>=u'0'&&c<=u'9')||c==u'_';
}
bool isControlCharacter(char16_t c){
  static const std::u16string control=u"&;()|<>";
  return control.find(c)!=std::u16string::npos;
}
std::u16string marker(){
  const auto uuid=platform::randomUUID();
  const auto text="BATBOX_SHELL_QUOTE_"+uuid+"_";
  return value::utf16CodeUnits(text);
}
std::u16string envValue(const EnvLookup& env,const std::u16string& key,
  const std::u16string& token){
  const auto name=fromUnits(key);
  const auto value=env?env(name):std::nullopt;
  if(!value)return key.empty()?u"$":u"";
  if(value->is_object()||value->is_array()||value->is_null()){
    auto result=token;
    result+=value::utf16CodeUnits(value::stringify(*value));result+=token;
    return result;
  }
  return value::utf16CodeUnits(jsPrimitive(*value));
}
std::u16string parseVariable(const std::u16string& s,std::size_t& i,
  const EnvLookup& env,const std::u16string& token){
  ++i;std::u16string name;
  const auto character=i<s.size()?s[i]:u'\0';
  if(character==u'{'){
    ++i;
    if(i<s.size()&&s[i]==u'}')
      throw std::runtime_error("Bad substitution: "+fromUnits(s.substr(i-2,3)));
    const auto end=s.find(u'}',i);
    if(end==std::u16string::npos)
      throw std::runtime_error("Bad substitution: "+fromUnits(s.substr(i)));
    name=s.substr(i,end-i);i=end;
  }else if(variableSpecial(character)){
    name+=character;++i;
  }else{
    const auto start=i;
    while(i<s.size()&&variableWord(s[i]))++i;
    name=s.substr(start,i-start);
    if(i!=s.size())--i;
  }
  return envValue(env,name,token);
}
void appendExpanded(Json& output,const std::u16string& text,const std::u16string& token){
  std::size_t position=0;
  while(position<text.size()){
    const auto begin=text.find(token,position);
    if(begin==std::u16string::npos){
      const auto tail=text.substr(position);if(!tail.empty())output.push_back(fromUnits(tail));
      return;
    }
    if(begin>position)output.push_back(fromUnits(text.substr(position,begin-position)));
    const auto end=text.find(token,begin+token.size());
    if(end==std::u16string::npos){output.push_back(fromUnits(text.substr(begin)));return;}
    output.push_back(value::parse(fromUnits(text.substr(begin+token.size(),end-begin-token.size()))));
    position=end+token.size();
  }
}
}
Json parse(const std::string& command,const EnvLookup& environment,const std::string& escape){
  const auto units=value::utf16CodeUnits(command);
  const std::string meta="|&;()<> \t";
  const std::string control="(?:\\|\\||\\&\\&|;;|\\|\\&|\\<\\(|\\<\\<\\<|>>|>\\&|<\\&|[&;()|<>])";
  const std::string bare="(\\"+escape+"['\""+meta+"]|[^\\s'\""+meta+"])+";
  const std::string doubleQuote="\"((\\\\\"|[^\"])*?)\"";
  const std::string singleQuote="'((\\\\'|[^'])*?)'";
  const std::wstring expression=widen("("+control+")|("+bare+"|"+doubleQuote+"|"+singleQuote+")+");
  const std::wregex chunker(expression,std::regex_constants::ECMAScript);
  const std::wregex controlPattern(widen("^"+control+"$"),std::regex_constants::ECMAScript);
  const auto transformed=regexInput(units);
  const auto token=marker();
  Json mapped=Json::array();bool commented=false;
  for(std::wsregex_iterator match(transformed.begin(),transformed.end(),chunker),end;
      match!=end;++match){
    const auto position=static_cast<std::size_t>(match->position());
    const auto s=units.substr(position,match->length());
    if(s.empty()||commented)continue;
    if(std::regex_match(match->str(),controlPattern)){
      mapped.push_back({{"op",fromUnits(s)}});continue;
    }
    char16_t quoteMode=0;bool escaped=false,glob=false,returned=false;
    std::u16string out;
    for(std::size_t i=0;i<s.size();++i){
      char16_t c=s[i];
      glob=glob||(!quoteMode&&(c==u'*'||c==u'?'));
      if(escaped){out+=c;escaped=false;}
      else if(quoteMode){
        if(c==quoteMode)quoteMode=0;
        else if(quoteMode==u'\'')out+=c;
        else{
          if(c==static_cast<char16_t>(escape.empty()?'\\':escape[0])){
            ++i;c=i<s.size()?s[i]:u'\0';
            if(c==u'"'||c==static_cast<char16_t>(escape.empty()?'\\':escape[0])||c==u'$')
              out+=c;
            else{out+=static_cast<char16_t>(escape.empty()?'\\':escape[0]);if(c)out+=c;}
          }else if(c==u'$')out+=parseVariable(s,i,environment,token);
          else out+=c;
        }
      }else if(c==u'"'||c==u'\'')quoteMode=c;
      else if(isControlCharacter(c)){
        mapped.push_back({{"op",fromUnits(s)}});returned=true;break;
      }else if(c==u'#'){
        commented=true;
        if(!out.empty())mapped.push_back(fromUnits(out));
        mapped.push_back({{"comment",fromUnits(units.substr(position+i+1))}});
        returned=true;break;
      }else if(c==static_cast<char16_t>(escape.empty()?'\\':escape[0]))escaped=true;
      else if(c==u'$')out+=parseVariable(s,i,environment,token);
      else out+=c;
    }
    if(returned)continue;
    if(glob)mapped.push_back({{"op","glob"},{"pattern",fromUnits(out)}});
    else mapped.push_back(fromUnits(out));
  }
  if(!environment)return mapped;
  Json expanded=Json::array();
  for(const auto& entry:mapped){
    if(!entry.is_string()){expanded.push_back(entry);continue;}
    const auto value=value::utf16CodeUnits(entry.get<std::string>());
    if(value.find(token)==std::u16string::npos){expanded.push_back(entry);continue;}
    appendExpanded(expanded,value,token);
  }
  return expanded;
}
} // namespace batbox::shell_quote
