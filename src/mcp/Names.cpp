#include "mcp/Mcp.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace batbox::mcp {
namespace {
std::optional<std::uint32_t> arrayIndex(const std::string& key){
  if(key.empty()||(key.size()>1&&key[0]=='0'))return std::nullopt;
  std::uint64_t value=0;
  for(const unsigned char c:key){
    if(c<'0'||c>'9')return std::nullopt;
    value=value*10+c-'0';
    if(value>=0xffffffffULL)return std::nullopt;
  }
  return static_cast<std::uint32_t>(value);
}
std::vector<std::string> ownKeys(const Json& object){
  std::vector<std::pair<std::uint32_t,std::string>> numeric;
  std::vector<std::string> other;
  for(auto it=object.begin();it!=object.end();++it){
    if(const auto index=arrayIndex(it.key()))numeric.emplace_back(*index,it.key());
    else other.push_back(it.key());
  }
  std::sort(numeric.begin(),numeric.end());
  std::vector<std::string> keys;keys.reserve(object.size());
  for(const auto& [_,key]:numeric)keys.push_back(key);
  keys.insert(keys.end(),other.begin(),other.end());
  return keys;
}
std::string jsString(const Json& value){
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_string())return value.get<std::string>();
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array()){
    std::string output;
    bool first=true;
    for(const auto& item:value){
      if(!first)output+=',';
      first=false;
      if(!item.is_null())output+=jsString(item);
    }
    return output;
  }
  return "[object Object]";
}
}
std::string jsStringValue(const Json& value){return jsString(value);}
std::vector<std::string> jsOwnKeys(const Json& value){return ownKeys(value);}
std::string normalizeNameForMCP(const std::string& name,const std::string& hostedPrefix){
  std::string output;
  for(const char16_t unit:value::utf16CodeUnits(name)){
    if((unit>='a'&&unit<='z')||(unit>='A'&&unit<='Z')||
       (unit>='0'&&unit<='9')||unit=='_'||unit=='-')output+=static_cast<char>(unit);
    else output+='_';
  }
  if(!hostedPrefix.empty()&&name.starts_with(hostedPrefix)){
    std::string collapsed;
    for(const char c:output)if(c!='_'||collapsed.empty()||collapsed.back()!='_')collapsed+=c;
    const auto first=collapsed.find_first_not_of('_');
    if(first==std::string::npos)return "";
    const auto last=collapsed.find_last_not_of('_');
    return collapsed.substr(first,last-first+1);
  }
  return output;
}
std::string buildMcpGadgetName(const std::string& serverName,const std::string& gadgetName,
  const std::string& hostedPrefix){
  return "mcp__"+normalizeNameForMCP(serverName,hostedPrefix)+"__"+
    normalizeNameForMCP(gadgetName,hostedPrefix);
}
std::string mcpGadgetInputToAutoClassifierInput(const Json& input,const std::string& gadgetName){
  if(!input.is_object()||input.empty())return gadgetName;
  std::string output;
  const auto append=[&](const std::string& key){
    if(!output.empty())output+=' ';
    output+=key+'='+jsString(input.at(key));
  };
  for(const auto& key:ownKeys(input))append(key);
  return output;
}
std::string inferCompactSchema(const Json& value,int depth){
  if(value.is_null())return "null";
  if(value.is_array())return value.empty()?"[]":"["+inferCompactSchema(value[0],depth-1)+"]";
  if(value.is_object()){
    if(depth<=0)return "{...}";
    std::string output="{";
    const auto keys=ownKeys(value);
    std::size_t count=0;
    for(const auto& key:keys){
      if(count>=10)break;
      if(count)output+=", ";
      output+=key+": "+inferCompactSchema(value.at(key),depth-1);
      ++count;
    }
    if(value.size()>10)output+=", ...";
    return output+"}";
  }
  if(value.is_string())return "string";
  if(value.is_boolean())return "boolean";
  return "number";
}
} // namespace batbox::mcp
