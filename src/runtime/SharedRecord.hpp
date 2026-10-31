#pragma once
#include "runtime/JsonValue.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace batbox::value {
using ValueRef=std::shared_ptr<Json>;
// JSON-shaped object properties with explicit undefined and nested identities.
// Replacing a property uses setRef; mutating its object/array uses its ValueRef.
class SharedRecord {
public:
  static SharedRecord fromJson(const Json& value){
    SharedRecord record;
    for(const auto& key:objectKeys(value))record.set(key,value.at(key));
    return record;
  }
  void set(std::string key,Json value){setRef(std::move(key),std::make_shared<Json>(std::move(value)));}
  void setRef(std::string key,ValueRef value){
    for(auto& entry:fields_)if(entry.first==key){entry.second=std::move(value);return;}
    fields_.emplace_back(std::move(key),std::move(value));
  }
  ValueRef get(const std::string& key)const{
    for(const auto& entry:fields_)if(entry.first==key)return entry.second;
    return {};
  }
  bool has(const std::string& key)const{
    for(const auto& entry:fields_)if(entry.first==key)return true;
    return false;
  }
  std::vector<std::string> keys()const{
    Json order=Json::object();for(const auto& entry:fields_)order[entry.first]=nullptr;
    return objectKeys(order);
  }
  SharedRecord shallowCopy()const{
    SharedRecord result;
    for(const auto& key:keys()){
      const auto value=get(key);
      result.setRef(key,!value||value->is_structured()?value:std::make_shared<Json>(*value));
    }
    return result;
  }
  Json snapshot()const{
    Json result=Json::object();
    for(const auto& key:keys())if(const auto value=get(key))result[key]=*value;
    return result;
  }
private:
  std::vector<std::pair<std::string,ValueRef>> fields_;
};
using RecordRef=std::shared_ptr<SharedRecord>;
} // namespace batbox::value
