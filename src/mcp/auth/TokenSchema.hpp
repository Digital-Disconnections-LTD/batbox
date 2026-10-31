#pragma once
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <cstdlib>
#include <optional>

namespace batbox::mcp::auth {
// Indigenous import for the SDK token schema. Its expires_in field uses numeric
// coercion; the provider itself continues to consume the validated wire value.
inline std::optional<Json> parseOAuthTokens(const Json& input){
  if(!input.is_object()||!value::property(input,"access_token").is_string()||
      !value::property(input,"token_type").is_string())return {};
  Json result={{"access_token",input.at("access_token")},{"token_type",input.at("token_type")}};
  for(const auto* key:{"id_token","scope","refresh_token"})if(input.contains(key)){
    if(!input.at(key).is_string())return {};
    result[key]=input.at(key);
  }
  if(input.contains("issuer")&&input.at("issuer").is_string())result["issuer"]=input.at("issuer");
  if(input.contains("expires_in")){
    const auto& value=input.at("expires_in");double number;
    if(value.is_number())number=value.get<double>();
    else if(value.is_boolean())number=value.get<bool>()?1:0;
    else if(value.is_null())number=0;
    else{
      if(!value.is_string())return {};
      const auto text=value::trim(value.get<std::string>());
      if(text.empty())number=0;
      else{char* end=nullptr;number=std::strtod(text.c_str(),&end);
        if(end!=text.c_str()+text.size())return {};}
    }
    if(!std::isfinite(number))return {};
    result["expires_in"]=number;
  }
  return result;
}
} // namespace batbox::mcp::auth
