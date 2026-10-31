#include "shell/ShellQuote.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line))try{
  const auto input=batbox::value::parse(line);
  Json result;
  if(input.at("op")=="quote"){
    std::vector<Json> arguments;
    for(const auto& arg:input.at("arguments"))arguments.push_back(arg);
    result=batbox::shell_quote::quote(arguments);
  }else if(input.at("op")=="parse"){
    batbox::shell_quote::EnvLookup lookup;
    if(input.contains("environment")){
      auto environment=input["environment"];
      lookup=[environment](const std::string& key)->std::optional<Json>{
        if(environment.contains(key))return environment[key];return std::nullopt;};
    }
    result=batbox::shell_quote::parse(input.at("command").get<std::string>(),lookup,
      input.contains("options")&&input["options"].contains("escape")?
        input["options"]["escape"].get<std::string>():"\\");
  }else throw std::logic_error("unknown op");
  std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
}catch(const std::exception& error){
  std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
}}
