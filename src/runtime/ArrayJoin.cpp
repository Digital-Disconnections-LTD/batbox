#include "runtime/ArrayJoin.hpp"
#include <stdexcept>
namespace batbox::value {
namespace {
std::string joinElement(const Json& value){
  if(value.is_null())return {};
  if(value.is_string())return value.get<std::string>();
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return numberToString(value.get<double>());
  if(value.is_array())return arrayJoin(value);
  // A JSON own toString property shadows the inherited callable method.
  if(value.contains("toString"))throw std::invalid_argument("Cannot convert object to primitive value");
  return "[object Object]";
}
}
std::string arrayJoin(const Json& values,const std::string& separator){
  if(!values.is_array())throw std::invalid_argument("Array join requires an array");
  std::string result;bool first=true;
  for(const auto& value:values){
    if(!first)result+=separator;
    first=false;result+=joinElement(value);
  }
  return result;
}
}
