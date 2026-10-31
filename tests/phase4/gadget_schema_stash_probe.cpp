#include "gadgets/SchemaStash.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    gadgets::clearGadgetSchemaStash();
    const bool identity=&gadgets::getGadgetSchemaStash()==
      &gadgets::getGadgetSchemaStash();
    Json values=Json::array();
    for(const auto& action:test["actions"]){
      const auto op=action["op"].get<std::string>();
      auto& stash=gadgets::getGadgetSchemaStash();
      if(op=="set")stash.set(action["key"].get<std::string>(),
        action["value"]);
      else if(op=="clear")gadgets::clearGadgetSchemaStash();
      else if(op=="get"){
        const auto value=stash.get(action["key"].get<std::string>());
        values.push_back(value?*value:Json(nullptr));
      }else if(op=="entries"){
        Json entries=Json::array();
        for(const auto& [key,value]:stash.entries())
          entries.push_back(Json::array({key,value}));
        values.push_back(entries);
      }
    }
    Json entries=Json::array();
    for(const auto& [key,value]:gadgets::getGadgetSchemaStash().entries())
      entries.push_back(Json::array({key,value}));
    std::cout<<value::stringify(Json{{"identity",identity},{"values",values},
      {"entries",entries}})<<'\n';
  }
}
