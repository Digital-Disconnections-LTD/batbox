#include "settings/Merge.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace settings=batbox::settings;
namespace value=batbox::value;
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto input=value::parse(line);
    const auto result=settings::mergeSettings(
      value::property(input,"target"),value::property(input,"source"));
    std::cout<<value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<value::stringify({{"error",error.what()}})<<'\n';
  }
}}
