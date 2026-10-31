#include "cli/structured/IndigenousSchemas.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace batbox;
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const Json test=value::parse(line);
    const auto kind=test.at("kind").get<std::string>();
    const auto& value=test.at("value");
    Json result;
    try{
      if(kind=="permissionUpdate"){
        const auto parsed=cli::structured::parsePermissionUpdate(value);
        if(!parsed)throw std::invalid_argument("Invalid input");
        result=*parsed;
      }else if(kind=="permission"){
        cli::structured::IndigenousSchemaServices services;
        services.logMalformedPermissionUpdates=[](const std::string&){};
        result=cli::structured::parsePermissionOutput(value,services);
      }else if(kind=="elicitation")
        result=cli::structured::parseElicitationResponse(value);
      else if(kind=="hook")result=cli::structured::parseHookOutput(value);
      else if(kind=="mcp")result=cli::structured::parseMcpResponse(value);
      else throw std::invalid_argument("Unknown schema");
      std::cout<<value::stringify(Json{{"success",true},{"data",result}})<<'\n';
    }catch(const std::exception&){
      std::cout<<"{\"success\":false}"<<'\n';
    }
  }
}
