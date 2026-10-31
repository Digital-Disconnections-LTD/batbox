#include "settings/Merge.hpp"

namespace batbox::settings {
namespace {
bool samePrimitive(const Json& left,const Json& right){
  if(left.is_structured()||right.is_structured())return false;
  return left==right;
}
Json merge(const Json& target,const Json& source){
  if(const auto arrays=settingsMergeCustomizer(target,source))return *arrays;
  if(source.is_object()){
    Json result=target.is_object()?target:Json::object();
    for(auto item=source.begin();item!=source.end();++item){
      const auto old=result.contains(item.key())?result[item.key()]:Json(nullptr);
      result[item.key()]=merge(old,item.value());
    }
    return result;
  }
  return source;
}
}
std::optional<Json> settingsMergeCustomizer(const Json& target,
  const Json& source){
  if(!target.is_array()||!source.is_array())return std::nullopt;
  Json result=Json::array();
  for(const auto* array:{&target,&source})for(const auto& item:*array){
    bool found=false;
    for(const auto& prior:result)if(samePrimitive(prior,item)){
      found=true;break;
    }
    if(!found)result.push_back(item);
  }
  return result;
}
Json mergeSettings(const Json& target,const Json& source){
  return merge(target,source);
}
}
