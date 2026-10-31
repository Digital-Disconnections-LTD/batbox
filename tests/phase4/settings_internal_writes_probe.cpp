#include "settings/persistence/InternalWrites.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto test=batbox::value::parse(line);
    double now=0;
    batbox::settings::persistence::InternalWrites writes([&]{return now;});
    batbox::Json values=batbox::Json::array();
    for(const auto& action:test.at("actions")){
      now=action.at("at").get<double>();
      const auto op=action.at("op").get<std::string>();
      if(op=="mark")writes.markInternalWrite(action.at("path"));
      else if(op=="consume")values.push_back(
        writes.consumeInternalWrite(action.at("path"),
          action.at("window").get<double>()));
      else if(op=="clear")writes.clearInternalWrites();
    }
    std::cout<<batbox::value::stringify(values)<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}}
