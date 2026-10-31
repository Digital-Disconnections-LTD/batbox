#include "instructions/hooks/runtime/HookOutputParser.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream stream(argv[1]);Json cases;stream>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    Json logs=Json::array();hook::HookParserServices services;
    services.schemaSafeParse=[](const Json& value){
      if(value.is_object()&&value.contains("bad")&&value.at("bad")==true)
        return hook::SchemaResult{std::nullopt,
          {{{"hookSpecificOutput"},"bad hook data"}}};
      return hook::SchemaResult{value,{}};
    };
    services.logDebug=[&](const std::string& message){logs.push_back(message);};
    services.stringifyThrown=[](std::exception_ptr){return std::string("Error: parse failed");};
    const auto kind=item.at("kind").get<std::string>();
    const auto body=item.at("body").get<std::string>();
    const auto parsed=kind=="command"?hook::parseHookOutput(body,services):
      hook::parseHttpHookOutput(body,services);
    Json result=Json::object();
    if(parsed.json)result["json"]=*parsed.json;
    if(parsed.plainText)result["plainText"]=*parsed.plainText;
    if(parsed.validationError)result["validationError"]=*parsed.validationError;
    rows.push_back({{"name",item.at("name")},{"result",result},{"logs",logs}});
  }
  std::cout<<value::stringify(rows)<<'\n';
}
