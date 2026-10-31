#include "skills/process/PoetryCommand.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::skills::process {
bool isEnvTruthy(const std::string& value){
  auto normalized=value::trim(value);
  for(auto& ch:normalized)if(ch>='A'&&ch<='Z')ch=static_cast<char>(ch+('a'-'A'));
  return normalized=="1"||normalized=="true"||normalized=="yes"||normalized=="on";
}
bool isSourceAdminTrusted(const Json& source){
  if(!source.is_string())return false;
  const auto value=source.get<std::string>();
  return value=="plugin"||value=="policySettings"||value=="built-in"||
    value=="builtin"||value=="bundled";
}
std::vector<std::string> parseGadgetListFromCLI(const Json& gadgets){
  std::vector<std::string> result;
  if(!gadgets.is_array())return result;
  for(const auto& item:gadgets){
    if(!item.is_string())continue;
    const auto gadgetString=item.get<std::string>();
    if(gadgetString.empty())continue;
    std::string current;
    bool isInParens=false;
    for(const auto ch:gadgetString){
      switch(ch){
      case '(':
        isInParens=true;current+=ch;break;
      case ')':
        isInParens=false;current+=ch;break;
      case ',':
        if(isInParens)current+=ch;
        else{if(!value::trim(current).empty())result.push_back(value::trim(current));current.clear();}
        break;
      case ' ':
        if(isInParens)current+=ch;
        else if(!value::trim(current).empty()){
          result.push_back(value::trim(current));current.clear();
        }
        break;
      default:current+=ch;break;
      }
    }
    if(!value::trim(current).empty())result.push_back(value::trim(current));
  }
  return result;
}
}
