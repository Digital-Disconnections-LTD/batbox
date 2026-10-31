#include "gadgets/bash/Hints.hpp"
#include "runtime/Decimal.hpp"
#include <algorithm>
#include <cmath>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace batbox::bash_gadget {
namespace {
std::size_t codepointBytes(unsigned char lead){
  return lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
}
std::size_t whitespaceBytes(std::string_view text,std::size_t position){
  if(position>=text.size())return 0;
  const auto length=std::min(codepointBytes(static_cast<unsigned char>(text[position])),text.size()-position);
  return value::trim(text.substr(position,length)).empty()?length:0;
}
bool word(char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
  (c>='0'&&c<='9')||c=='_';}
bool lineTerminatorAt(std::string_view text,std::size_t position){
  if(position>=text.size())return false;
  if(text[position]=='\n'||text[position]=='\r')return true;
  return text.substr(position,3)=="\xe2\x80\xa8"||text.substr(position,3)=="\xe2\x80\xa9";
}
bool lineStartAt(std::string_view text,std::size_t position){
  return position==0||text[position-1]=='\n'||text[position-1]=='\r'||
    (position>=3&&(text.substr(position-3,3)=="\xe2\x80\xa8"||
      text.substr(position-3,3)=="\xe2\x80\xa9"));
}
std::unordered_map<std::string,std::string> parseAttrs(std::string_view raw){
  std::unordered_map<std::string,std::string> attrs;
  for(std::size_t i=0;i<raw.size();){
    if(!word(raw[i])){++i;continue;}
    std::size_t end=i;while(end<raw.size()&&word(raw[end]))++end;
    if(end>=raw.size()||raw[end]!='='){++i;continue;}
    const auto key=std::string(raw.substr(i,end-i));
    const auto valueStart=end+1;
    if(valueStart>=raw.size()){++i;continue;}
    if(raw[valueStart]=='"'){
      const auto close=raw.find('"',valueStart+1);
      if(close!=std::string_view::npos){
        attrs[key]=std::string(raw.substr(valueStart+1,close-valueStart-1));
        i=close+1;continue;
      }
    }
    std::size_t valueEnd=valueStart;
    while(valueEnd<raw.size()&&raw[valueEnd]!='/'&&raw[valueEnd]!='>'&&
      whitespaceBytes(raw,valueEnd)==0)
      valueEnd+=codepointBytes(static_cast<unsigned char>(raw[valueEnd]));
    if(valueEnd==valueStart){++i;continue;}
    attrs[key]=std::string(raw.substr(valueStart,valueEnd-valueStart));
    i=valueEnd;
  }
  return attrs;
}
bool versionOne(const std::unordered_map<std::string,std::string>& attrs){
  const auto found=attrs.find("v");if(found==attrs.end())return false;
  const auto value=value::trim(found->second);
  if(value.size()>=3&&value[0]=='0'&&
     (value[1]=='x'||value[1]=='X'||value[1]=='b'||value[1]=='B'||
      value[1]=='o'||value[1]=='O')){
    const int base=value[1]=='x'||value[1]=='X'?16:value[1]=='b'||value[1]=='B'?2:8;
    bool any=false;std::size_t nonzero=value.size();
    for(std::size_t i=2;i<value.size();++i){
      const auto c=value[i];int digit=-1;
      if(c>='0'&&c<='9')digit=c-'0';
      else if(c>='a'&&c<='f')digit=c-'a'+10;
      else if(c>='A'&&c<='F')digit=c-'A'+10;
      if(digit<0||digit>=base)return false;
      any=true;if(digit!=0&&nonzero==value.size())nonzero=i;
    }
    return any&&nonzero+1==value.size()&&value[nonzero]=='1';
  }
  static const std::regex decimal(R"(^[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?$)");
  if(!std::regex_match(value,decimal))return false;
  return value::detail::decimalNumber(value)==1;
}
std::string firstCommandToken(const std::string& command){
  const auto trimmed=value::trim(command);
  for(std::size_t i=0;i<trimmed.size();){
    if(whitespaceBytes(trimmed,i))return trimmed.substr(0,i);
    i+=codepointBytes(static_cast<unsigned char>(trimmed[i]));
  }
  return trimmed;
}
std::size_t matchTag(std::string_view text,std::size_t start,const std::string& tagName){
  std::size_t i=start;
  while(i<text.size()&&(text[i]==' '||text[i]=='\t'))++i;
  const auto open="<"+tagName;
  if(text.substr(i,open.size())!=open)return std::string_view::npos;
  i+=open.size();
  auto blank=whitespaceBytes(text,i);
  if(!blank)return std::string_view::npos;
  i+=blank;
  const auto close=text.find('>',i);
  if(close==std::string_view::npos||close==0||text[close-1]!='/')return std::string_view::npos;
  i=close+1;
  while(i<text.size()&&(text[i]==' '||text[i]=='\t'))++i;
  if(i<text.size()&&!lineTerminatorAt(text,i))return std::string_view::npos;
  return i;
}
std::string collapseBlankLines(std::string_view text){
  std::string result;result.reserve(text.size());
  for(std::size_t i=0;i<text.size();){
    if(text[i]!='\n'){result.push_back(text[i++]);continue;}
    std::size_t end=i;while(end<text.size()&&text[end]=='\n')++end;
    result.append(std::min<std::size_t>(2,end-i),'\n');i=end;
  }
  return result;
}
}
Json extractBatboxHints(const std::string& output,const std::string& command,
                        const HintProtocol& protocol){
  if(protocol.tagName.empty()||protocol.tagName.find_first_of("<>\n\r\t ")!=std::string::npos)
    throw std::invalid_argument("hint protocol tagName required");
  if(!protocol.logDebug)throw std::invalid_argument("hint protocol logDebug required");
  Json hints=Json::array();
  if(output.find("<"+protocol.tagName)==std::string::npos)
    return {{"hints",hints},{"stripped",output}};
  const auto sourceCommand=firstCommandToken(command);
  std::string stripped;std::size_t cursor=0;
  for(std::size_t start=0;start<=output.size();++start){
    if(!lineStartAt(output,start))continue;
    const auto end=matchTag(output,start,protocol.tagName);
    if(end==std::string::npos)continue;
    stripped.append(output,cursor,start-cursor);
    const auto attrs=parseAttrs(std::string_view(output).substr(start,end-start));
    auto get=[&](const char* key)->std::string{const auto it=attrs.find(key);
      return it==attrs.end()?"":it->second;};
    auto debugField=[&](const char* key)->std::string{const auto it=attrs.find(key);
      return it==attrs.end()?"undefined":it->second;};
    const auto type=get("type"),value=get("value");
    if(!versionOne(attrs))
      protocol.logDebug("[batboxHints] dropped hint with unsupported v="+debugField("v"));
    else if(type!="plugin")
      protocol.logDebug("[batboxHints] dropped hint with unsupported type="+debugField("type"));
    else if(value.empty())
      protocol.logDebug("[batboxHints] dropped hint with empty value");
    else hints.push_back({{"v",1},{"type",type},{"value",value},
      {"sourceCommand",sourceCommand}});
    cursor=end;start=end;
  }
  stripped.append(output,cursor,std::string::npos);
  if(stripped!=output)stripped=collapseBlankLines(stripped);
  return {{"hints",hints},{"stripped",stripped}};
}
} // namespace batbox::bash_gadget
