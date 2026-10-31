#include "indigenous_yoga/Types.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>

namespace batbox::indigenous_yoga {
Value pointValue(double value){return {Unit::Point,value};}
Value percentValue(double value){return {Unit::Percent,value};}
double resolveValue(Value value,double ownerSize){
  switch(value.unit){
    case Unit::Point:return value.value;
    case Unit::Percent:return std::isnan(ownerSize)?NaN:
      value.value*ownerSize/100;
    default:return NaN;
  }
}
bool isDefined(double value){return !std::isnan(value);}
bool sameFloat(double a,double b){return a==b||(a!=a&&b!=b);}
Style defaultStyle(){return {};}
ConfigRef createConfig(){return std::make_shared<Config>();}
ConfigRef defaultConfig(){
  static auto config=createConfig();return config;
}
Value parseDimension(const std::variant<double,std::string,std::monostate>& v){
  if(std::holds_alternative<std::monostate>(v))return UNDEFINED_VALUE;
  if(const auto number=std::get_if<double>(&v))
    return std::isfinite(*number)?pointValue(*number):UNDEFINED_VALUE;
  const auto& string=std::get<std::string>(v);
  if(string=="auto")return AUTO_VALUE;
  if(string.ends_with('%'))return percentValue(value::parseFloat(string));
  const auto number=value::parseFloat(string);
  return std::isnan(number)?UNDEFINED_VALUE:pointValue(number);
}
} // namespace batbox::indigenous_yoga
