#include "shell/CommandSplitter.hpp"
#include "runtime/JsonValue.hpp"
#include <optional>
#include <unordered_set>

namespace batbox::shell_split {
namespace {
bool descriptor(const std::string& text){return text=="0"||text=="1"||text=="2";}
bool whitespace(char16_t c){
  const auto text=value::fromUtf16(std::u16string_view(&c,1));return value::trim(text).empty();
}
bool starts(const std::string& text,char c){return !text.empty()&&text[0]==c;}
bool includes(const std::string& text,char c){return text.find(c)!=std::string::npos;}
}
bool isStaticRedirectTarget(const std::string& target){
  const auto units=value::utf16CodeUnits(target);
  for(char16_t c:units)if(whitespace(c)||c==u'\''||c==u'"')return false;
  if(target.empty()||starts(target,'#'))return false;
  return !starts(target,'!')&&!starts(target,'=')&&!includes(target,'$')&&
    !includes(target,'`')&&!includes(target,'*')&&!includes(target,'?')&&
    !includes(target,'[')&&!includes(target,'{')&&!includes(target,'~')&&
    !includes(target,'(')&&!includes(target,'<')&&!starts(target,'&');
}
std::vector<std::string> filterControlOperators(const std::vector<std::string>& parts){
  static const std::unordered_set<std::string> operators={"&&","||",";",";;","|",">&",">",">>"};
  std::vector<std::string> result;
  for(const auto& part:parts)if(!operators.contains(part))result.push_back(part);
  return result;
}
std::vector<std::string> splitCommandDeprecated(const std::string& command,
  const SaltProvider& saltProvider){
  const auto raw=splitCommandWithOperators(command,saltProvider);
  std::vector<std::optional<std::string>> parts;
  for(const auto& part:raw)parts.push_back(part);
  for(std::size_t i=0;i<parts.size();++i){
    if(!parts[i])continue;
    const auto part=*parts[i];
    if(part!=">&"&&part!=">"&&part!=">>")continue;
    const auto prev=i>0&&parts[i-1]?std::optional(value::trim(*parts[i-1])):std::nullopt;
    const auto next=i+1<parts.size()&&parts[i+1]?std::optional(value::trim(*parts[i+1])):std::nullopt;
    const auto after=i+2<parts.size()&&parts[i+2]?std::optional(value::trim(*parts[i+2])):std::nullopt;
    if(!next)continue;
    bool strip=false,stripThird=false;
    std::string effective=*next;
    if((part==">"||part==">>")&&next->size()>=3&&
       (*next)[next->size()-2]==' '&&descriptor(next->substr(next->size()-1))&&
       after&&(*after==">"||*after==">>"||*after==">&"))
      effective=next->substr(0,next->size()-2);
    if(part==">&"&&descriptor(*next))strip=true;
    else if(part==">"&&*next=="&"&&after&&descriptor(*after)){
      strip=true;stripThird=true;
    }else if(part==">"&&starts(*next,'&')&&next->size()>1&&descriptor(next->substr(1)))
      strip=true;
    else if((part==">"||part==">>")&&isStaticRedirectTarget(effective))strip=true;
    if(!strip)continue;
    if(prev&&!prev->empty()&&prev->size()>=3&&
       descriptor(prev->substr(prev->size()-1))&&(*prev)[prev->size()-2]==' ')
      parts[i-1]=prev->substr(0,prev->size()-2);
    parts[i]=std::nullopt;parts[i+1]=std::nullopt;
    if(stripThird)parts[i+2]=std::nullopt;
  }
  std::vector<std::string> stringParts;
  for(const auto& part:parts)if(part&&!part->empty())stringParts.push_back(*part);
  return filterControlOperators(stringParts);
}
} // namespace batbox::shell_split
