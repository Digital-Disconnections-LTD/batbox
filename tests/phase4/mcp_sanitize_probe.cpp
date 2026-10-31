#include "mcp/Mcp.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto result=input.at("op")=="string"
      ?batbox::Json(batbox::mcp::partiallySanitizeUnicode(input.at("value").get<std::string>()))
      :batbox::mcp::recursivelySanitizeUnicode(input.at("value"));
    std::cout<<batbox::value::stringify(batbox::Json{{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(batbox::Json{{"error",error.what()}})<<'\n';
  }
}
