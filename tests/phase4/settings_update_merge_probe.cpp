#include "settings/persistence/Update.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto input=batbox::value::parse(line);
    batbox::settings::persistence::UpdatePatch patch;
    patch.value=input.at("patch");
    for(const auto& path:input.at("deletes"))
      patch.undefinedPaths.push_back(path.get<std::vector<std::string>>());
    const auto existing=input.contains("existing")?
      input["existing"]:batbox::Json::object();
    const auto result=batbox::settings::persistence::mergeForUpdate(
      existing,patch);
    std::cout<<batbox::value::stringify({{"written",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}}
