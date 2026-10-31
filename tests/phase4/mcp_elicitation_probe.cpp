#include "mcp/Call.hpp"
#include "mcp/StdioClient.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    int calls=0;
    try{
      const auto input=batbox::value::parse(line);
      batbox::mcp::Connection connection;
      connection.connected=true;connection.name=input.value("server",std::string("demo"));
      connection.callGadget=[&](const std::string&,const batbox::Json&,std::stop_token,
          const std::function<void(const batbox::Json&)>&){
        ++calls;
        if(calls<=input.value("failures",0))
          throw batbox::mcp::ProtocolError(input.value("code",-32042),
            input.value("message",std::string("elicitation required")),
            {{"elicitations",input.value("elicitations",batbox::Json())}});
        return batbox::mcp::GadgetCallResult{"done"};};
      batbox::mcp::UrlElicitationServices services;
      services.invokeOnOwner=[](std::function<void()> callback){callback();};
      services.runHooks=[input](const std::string&,const batbox::Json& elicitation,
          std::stop_token)->std::optional<batbox::Json>{
        const auto& hooks=batbox::value::property(input,"hook");
        const auto id=elicitation.at("elicitationId").get<std::string>();
        if(hooks.is_object()&&hooks.contains(id))return hooks.at(id);
        return std::nullopt;};
      services.handle=[input](const std::string&,const batbox::Json&,std::stop_token){
        return input.value("user",batbox::Json{{"action","accept"}});};
      services.runResultHooks=[input](const std::string&,const batbox::Json& user,
          std::stop_token,const std::string&,const std::string& id){
        const auto& finals=batbox::value::property(input,"final");
        return finals.is_object()&&finals.contains(id)?finals.at(id):user;};
      const auto result=batbox::mcp::callMCPGadgetWithUrlElicitationRetry(connection,
        input.value("tool",std::string("test")),batbox::Json::object(),{}, {},services);
      std::cout<<batbox::value::stringify({{"result",{{"content",result.content}}},{"calls",calls}})<<'\n';
    }catch(const std::exception& error){
      std::cout<<batbox::value::stringify({{"error",error.what()},{"calls",calls}})<<'\n';
    }
  }
}
