#include "settings/schema/imports/Sandbox.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto result=batbox::settings::schema::imports::sandboxSettings(
      batbox::value::parse(line));
    std::cout<<batbox::value::stringify(result.success?
      batbox::Json{{"success",true},{"data",result.data}}:
      batbox::Json{{"success",false},{"issues",result.issues}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}}
