#include "terminal_dom/style/StyleValues.hpp"
#include <cmath>

namespace batbox::terminal_dom::style::detail {
const Json* field(const Json& object,const char* key){
  if(!object.is_object())return nullptr;
  const auto it=object.find(key);
  return it==object.end()?nullptr:&*it;
}
bool undefined(const Json& value){
  if(value.is_discarded())return true;
  if(!value.is_object()||value.size()!=1)return false;
  const auto it=value.find("$undefined");
  return it!=value.end()&&it->is_boolean()&&it->get<bool>();
}
bool nullish(const Json& value){return value.is_null()||undefined(value);}
bool number(const Json& value){return value.is_number();}
bool string(const Json& value){return value.is_string();}
bool truthy(const Json& value){
  if(nullish(value))return false;
  if(value.is_boolean())return value.get<bool>();
  if(value.is_number()){
    const double n=value.get<double>();return n!=0&&!std::isnan(n);
  }
  if(value.is_string())return !value.get_ref<const std::string&>().empty();
  return true;
}
double asNumber(const Json& value){
  if(nullish(value))return value.is_null()?0:NaN;
  if(value.is_number())return value.get<double>();
  if(value.is_boolean())return value.get<bool>()?1:0;
  if(value.is_string()){
    const auto& str=value.get_ref<const std::string&>();
    if(str.empty())return 0;
    try{std::size_t used=0;const auto n=std::stod(str,&used);
      return used==str.size()?n:NaN;
    }catch(...){return NaN;}
  }
  return NaN;
}
double numberOr(const Json* value,double fallback){
  return !value||nullish(*value)?fallback:asNumber(*value);
}
double parseInt10(const std::string& value){
  std::size_t i=0;
  while(i<value.size()&&static_cast<unsigned char>(value[i])<=0x20)++i;
  bool negative=false;
  if(i<value.size()&&(value[i]=='+'||value[i]=='-')){
    negative=value[i]=='-';++i;
  }
  const auto start=i;
  long double result=0;
  while(i<value.size()&&value[i]>='0'&&value[i]<='9'){
    result=result*10+(value[i]-'0');++i;
  }
  if(i==start)return NaN;
  const double converted=static_cast<double>(result);
  return negative?-converted:converted;
}
std::string asString(const Json& value){
  return value.is_string()?value.get<std::string>():std::string{};
}
} // namespace batbox::terminal_dom::style::detail
