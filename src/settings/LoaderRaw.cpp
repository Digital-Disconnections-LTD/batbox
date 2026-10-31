#include "settings/Loader.hpp"
#include "filesystem/FileOps.hpp"
#include <array>
#include <charconv>

namespace batbox::settings {
namespace {
bool inheritedObjectKey(const std::string& key){
  constexpr std::array<const char*,12> names={"constructor","toString",
    "toLocaleString","valueOf","hasOwnProperty","isPrototypeOf",
    "propertyIsEnumerable","__defineGetter__","__defineSetter__",
    "__lookupGetter__","__lookupSetter__","__proto__"};
  for(const auto name:names)if(key==name)return true;
  return false;
}
bool hasIn(const Json& value,const std::string& key){
  if(inheritedObjectKey(key))return true;
  if(value.is_object())return value.contains(key);
  if(value.is_array()){
    if(key=="length"||key=="at"||key=="concat"||key=="find"||
       key=="filter"||key=="map"||key=="join"||key=="push"||
       key=="pop"||key=="slice"||key=="splice")return true;
    std::size_t index=0;
    const auto [end,error]=std::from_chars(key.data(),key.data()+key.size(),
      index);
    return error==std::errc{}&&end==key.data()+key.size()&&
      std::to_string(index)==key&&index<value.size();
  }
  return false;
}
}
bool Loader::rawSettingsContainsKey(const std::string& key){
  for(const auto source:getEnabledSettingSources(services_.allowedSources)){
    if(source==Source::Policy)continue;
    const auto path=services_.paths->getSettingsFilePathForSource(source);
    if(!path)continue;
    try{
      const auto resolved=filesystem::safeResolvePath(*path);
      const auto content=filesystem::readFileSyncWithMetadata(
        resolved.resolvedPath).content;
      if(value::trim(content).empty())continue;
      const auto jsonText=content.starts_with("\xef\xbb\xbf")
        ?content.substr(3):content;
      auto data=value::parse(jsonText,false);
      if(data.is_discarded())data=nullptr;
      if((data.is_object()||data.is_array())&&hasIn(data,key))return true;
    }catch(const std::exception& error){
      if(services_.parser.logFileError)
        services_.parser.logFileError(*path,error.what());
    }
  }
  return false;
}
}
