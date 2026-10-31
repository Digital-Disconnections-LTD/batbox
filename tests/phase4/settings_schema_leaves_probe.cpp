#include "settings/schema/Leaves.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace schema=batbox::settings::schema;
namespace value=batbox::value;
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto input=value::parse(line);
    const auto op=value::property(input,"op").get<std::string>();
    const auto& value=value::property(input,"value");
    batbox::settings::SchemaResult result;
    if(op=="EnvironmentVariablesSchema")
      result=schema::environmentVariables(value);
    else if(op=="PermissionsSchema")
      result=schema::permissions(value,
        {value::truthy(value::property(input,"classifier"))},
        batbox::settings::PermissionRuleServices{});
    else if(op=="AllowedMcpServerEntrySchema")
      result=schema::allowedMcpServerEntry(value);
    else if(op=="DeniedMcpServerEntrySchema")
      result=schema::deniedMcpServerEntry(value);
    else throw std::invalid_argument("Unknown schema");
    std::cout<<value::stringify(result.success?Json{{"success",true},
      {"data",result.data}}:Json{{"success",false},
      {"issues",result.issues}})<<'\n';
  }catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
