#pragma once
#include "model_config/ModelConfig.hpp"
#include "utils/envUtils.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace batbox::model_config::internal {
inline bool has(const Json& value,const std::string& key){return value.is_object()&&value.contains(key);}
inline Json get(const Json& value,const std::string& key){return has(value,key)?value.at(key):Json();}
inline std::string str(const Json& value){return value.is_string()?value.get<std::string>():std::string();}
inline std::string field(const Json& value,const std::string& key){return str(get(value,key));}
inline std::string lower(std::string x){for(char& c:x)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return x;}
inline bool contains(const std::string& x,const std::string& needle){return x.find(needle)!=std::string::npos;}
inline Json envValue(const Services& s,const std::string& key){return get(s.env,key);}
inline std::string envString(const Services& s,const std::string& key){return str(envValue(s,key));}
inline bool envTruthy(const Services& s,const std::string& key){return batbox::isEnvTruthy(envValue(s,key));}
inline std::string pattern(const Services& s,const std::string& key){
  const auto value=field(s.protocol,key);
  if(value.empty())throw std::logic_error("model protocol pattern "+key+" not supplied");
  return value;
}
inline std::string provider(const Services& s){
  if(s.provider.empty())throw std::logic_error("provider dependency not supplied");
  return s.provider;
}
template<class Fn>inline void require(const Fn& fn,const char* name){if(!fn)throw std::logic_error(std::string(name)+" dependency not supplied");}
inline bool internalUser(const Services& s){return envString(s,"BATBOX_USER_TYPE")=="internal";}
} // namespace batbox::model_config::internal
