#include "settings/schema/Internal.hpp"

namespace batbox::settings::schema {
SchemaResult environmentVariables(const Json& input){
  Json issues=Json::array();
  if(!input.is_object()){
    detail::invalidType(issues,Json::array(),"record",input);
    return {false,nullptr,issues};
  }
  Json result=Json::object();
  for(auto it=input.begin();it!=input.end();++it)
    result[it.key()]=detail::jsString(it.value());
  return {true,result,issues};
}
}
