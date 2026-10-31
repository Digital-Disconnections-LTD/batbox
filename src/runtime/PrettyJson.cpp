#include "runtime/JsonValue.hpp"
#include <algorithm>

namespace batbox::value {
namespace {
std::string pretty(const Json& value,int space,int depth){
  if(!value.is_array()&&!value.is_object())return stringify(value);
  if(value.empty())return value.is_array()?"[]":"{}";
  const auto indent=[space](int level){return std::string(space*level,' ');};
  const bool array=value.is_array();std::string output=array?"[\n":"{\n";
  if(array){
    for(std::size_t i=0;i<value.size();++i){
      if(i)output+=",\n";
      output+=indent(depth+1)+pretty(value[i],space,depth+1);
    }
  }else{
    const auto keys=objectKeys(value);
    for(std::size_t i=0;i<keys.size();++i){
      if(i)output+=",\n";
      output+=indent(depth+1)+quote(keys[i])+": "+pretty(value.at(keys[i]),space,depth+1);
    }
  }
  return output+"\n"+indent(depth)+(array?"]":"}");
}
}
std::string stringify(const Json& value,int space){
  space=std::clamp(space,0,10);return space?pretty(value,space,0):stringify(value);
}
} // namespace batbox::value
