#include "react_sdk/Value.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <unordered_set>

namespace batbox::react_sdk {
bool Props::hasOwn(const std::string& name)const{
  return std::any_of(fields.begin(),fields.end(),[&](const Property& field){
    return field.name==name;});
}
bool Props::has(const std::string& name)const{
  return hasOwn(name)||(prototype&&prototype->has(name));
}
ValueRef Props::getOwn(const std::string& name)const{
  for(const auto& field:fields)if(field.name==name)return field.value;
  return undefined();
}
ValueRef Props::get(const std::string& name)const{
  if(hasOwn(name))return getOwn(name);
  return prototype?prototype->get(name):undefined();
}
void Props::set(std::string name,ValueRef value,bool enumerable){
  for(auto& field:fields)if(field.name==name){
    field.value=std::move(value);field.enumerable=enumerable;return;
  }
  fields.push_back({std::move(name),std::move(value),enumerable});
}
std::vector<std::string> Props::ownEnumerableKeys()const{
  Json order=Json::object();
  for(const auto& field:fields)if(field.enumerable)order[field.name]=nullptr;
  return value::objectKeys(order);
}
std::vector<std::string> Props::enumerableKeys()const{
  auto keys=ownEnumerableKeys();
  std::unordered_set<std::string> seen;
  for(const auto& field:fields)seen.insert(field.name);
  if(prototype)for(const auto& key:prototype->enumerableKeys())
    if(seen.insert(key).second)keys.push_back(key);
  return keys;
}
}
