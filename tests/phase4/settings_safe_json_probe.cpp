#include "settings/persistence/SafeJson.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto test=batbox::value::parse(line);
    int logs=0;
    batbox::settings::persistence::SafeJsonParser parser(
      [&](const std::string&){++logs;});
    batbox::Json output=batbox::Json::array();
    for(const auto& action:test.at("actions")){
      if(action.value("clear",false)){parser.clear();continue;}
      const std::optional<std::string> text=action.contains("text")&&
        action["text"].is_string()?
        std::optional(action["text"].get<std::string>()):std::nullopt;
      const auto value=parser.safeParseJSON(text,action.value("log",true));
      output.push_back({{"value",value},{"cacheSize",parser.stashSize()},
        {"logCount",logs}});
    }
    std::cout<<batbox::value::stringify(output)<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}}
