#include "interactive/lifecycle/BashGadgets.hpp"
#include <algorithm>

namespace batbox::interactive::lifecycle {
namespace {
bool space(char16_t c){
  return (c>=u'\t'&&c<=u'\r')||c==u' '||c==0x00a0||c==0x1680||
    (c>=0x2000&&c<=0x200a)||c==0x2028||c==0x2029||c==0x202f||
    c==0x205f||c==0x3000||c==0xfeff;
}
bool nameStart(char16_t c){return (c>=u'A'&&c<=u'Z')||
  (c>=u'a'&&c<=u'z')||c==u'_';}
bool nameRest(char16_t c){return nameStart(c)||(c>=u'0'&&c<=u'9');}
bool assignment(std::u16string_view token){
  if(token.empty()||!nameStart(token.front()))return false;
  std::size_t index=1;
  while(index<token.size()&&nameRest(token[index]))++index;
  return index<token.size()&&token[index]==u'=';
}
const Json& field(const Json& value,const char* name){return value::property(value,name);}
}
std::optional<std::string> extractCliName(
    const std::optional<std::string>& command){
  if(!command||command->empty())return {};
  auto code=value::utf16CodeUnits(*command);
  std::size_t start=0,end=code.size();
  while(start<end&&space(code[start]))++start;
  while(end>start&&space(code[end-1]))--end;
  if(start==end)return std::string();
  for(std::size_t index=start;index<end;){
    auto after=index;
    while(after<end&&!space(code[after]))++after;
    const std::u16string_view token(code.data()+index,after-index);
    if(!assignment(token)&&token!=u"sudo")
      return value::fromUtf16(token);
    index=after;
    while(index<end&&space(code[index]))++index;
  }
  return {};
}
runtime::OrderedStringSet extractBashGadgetsFromMessages(
    const TurnMessagesRef& messages,std::size_t first){
  runtime::OrderedStringSet gadgets;
  if(!messages)return gadgets;
  for(std::size_t index=std::min(first,messages->size());
      index<messages->size();++index){
    const auto& message=*(*messages)[index];
    if(field(message,"type")!="assistant")continue;
    const auto& content=field(field(message,"message"),"content");
    if(!content.is_array())continue;
    for(const auto& block:content){
      if(field(block,"type")!="tool_use"||field(block,"name")!="Bash")
        continue;
      const auto& input=field(block,"input");
      if(!input.is_object()||!input.contains("command"))continue;
      const auto& command=field(input,"command");
      const auto cli=extractCliName(command.is_string()?
        std::optional<std::string>(command.get<std::string>()):std::nullopt);
      if(cli&&!cli->empty())gadgets.insert(*cli);
    }
  }
  return gadgets;
}
} // namespace batbox::interactive::lifecycle
