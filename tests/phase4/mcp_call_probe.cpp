#include "mcp/Call.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    batbox::Json result;
    if(input.at("op")=="timeout"){
      const auto env=input.value("env",batbox::Json());
      result=batbox::mcp::getMcpGadgetTimeoutMs(env.is_string()
        ?std::optional<std::string>(env.get<std::string>()):std::nullopt);
    }else result=batbox::mcp::isMcpFlightExpiredError(
      input.value("code",0),input.value("message",std::string()));
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
