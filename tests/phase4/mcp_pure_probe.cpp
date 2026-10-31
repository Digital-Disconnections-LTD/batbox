#include "mcp/Mcp.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    Json result;
    if(input.at("op")=="normalize")result=batbox::mcp::normalizeNameForMCP(
      input.at("name").get<std::string>(),input.at("hostedPrefix").get<std::string>());
    else if(input.at("op")=="build")result=batbox::mcp::buildMcpGadgetName(
      input.at("server").get<std::string>(),input.at("tool").get<std::string>(),
      input.at("hostedPrefix").get<std::string>());
    else if(input.at("op")=="classifier")result=batbox::mcp::mcpGadgetInputToAutoClassifierInput(
      input.at("input"),input.at("tool").get<std::string>());
    else if(input.at("op")=="schema")result=batbox::mcp::inferCompactSchema(
      input.at("value"),input.value("depth",2));
    else throw std::runtime_error("unknown operation");
    std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
  }
}
