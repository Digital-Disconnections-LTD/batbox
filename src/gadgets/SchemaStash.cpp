#include "gadgets/SchemaStash.hpp"
#include <utility>

namespace batbox::gadgets {
void SchemaStash::set(std::string name,Json schema){
  for(auto& [key,value]:entries_){
    if(key==name){value=std::move(schema);return;}
  }
  entries_.emplace_back(std::move(name),std::move(schema));
}
std::optional<Json> SchemaStash::get(const std::string& name)const{
  for(const auto& [key,value]:entries_)
    if(key==name)return value;
  return std::nullopt;
}
const std::vector<std::pair<std::string,Json>>&
SchemaStash::entries()const{return entries_;}
void SchemaStash::clear(){entries_.clear();}
SchemaStash& getGadgetSchemaStash(){
  static SchemaStash stash;
  return stash;
}
void clearGadgetSchemaStash(){getGadgetSchemaStash().clear();}
}
