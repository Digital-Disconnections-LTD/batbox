#include "settings/persistence/Update.hpp"
#include <charconv>
#include <stdexcept>

namespace batbox::settings::persistence {
namespace {
using Path=std::vector<std::string>;
bool deleted(const std::vector<Path>& paths,const Path& at){
  for(const auto& path:paths)if(path==at)return true;
  return false;
}
void insertUndefined(Json& value,const Path& path,std::size_t index){
  if(index==path.size())return;
  if(!value.is_object())value=Json::object();
  const auto& key=path[index];
  if(index+1==path.size()){
    if(!value.contains(key))value[key]=nullptr;
    return;
  }
  if(!value.contains(key))value[key]=Json::object();
  insertUndefined(value[key],path,index+1);
}
Json mergeNode(const Json& existing,const Json& patch,
  const std::vector<Path>& paths,Path at){
  if(patch.is_array())return patch;
  if(patch.is_object()){
    if(existing.is_array()){
      Json output=existing;
      for(auto it=patch.begin();it!=patch.end();++it){
        const auto& key=it.key();
        std::size_t index=0;
        const auto parsed=std::from_chars(key.data(),key.data()+key.size(),index);
        if(parsed.ec!=std::errc()||parsed.ptr!=key.data()+key.size()||
            std::to_string(index)!=key||index>=4294967295ULL)continue;
        while(output.size()<=index)output.push_back(nullptr);
        at.push_back(key);
        if(deleted(paths,at))output[index]=nullptr;
        else output[index]=mergeNode(output[index],it.value(),paths,at);
        at.pop_back();
      }
      return output;
    }
    Json output=existing.is_object()?existing:Json::object();
    for(auto it=patch.begin();it!=patch.end();++it){
      at.push_back(it.key());
      if(deleted(paths,at))output.erase(it.key());
      else output[it.key()]=mergeNode(output.contains(it.key())?
        output[it.key()]:Json(nullptr),it.value(),paths,at);
      at.pop_back();
    }
    return output;
  }
  return patch;
}
}
Json mergeForUpdate(const Json& existing,const UpdatePatch& patch){
  Json source=patch.value;
  for(const auto& path:patch.undefinedPaths){
    if(path.empty())throw std::invalid_argument("Empty undefined settings path");
    insertUndefined(source,path,0);
  }
  return mergeNode(existing,source,patch.undefinedPaths,{});
}
}
