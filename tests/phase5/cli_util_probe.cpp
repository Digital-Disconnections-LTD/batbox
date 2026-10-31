#include "cli/util/Exit.hpp"
#include "cli/util/Ndjson.hpp"
#include <iostream>
using namespace batbox;
int main(){std::string line;while(std::getline(std::cin,line)){
 const auto input=Json::parse(line);Json trace=Json::array();
 cli::ExitServices exits{[&](const auto& v){trace.push_back({"error",v});},
  [&](const auto& v){trace.push_back({"write",v});},
  [&](int code){trace.push_back({"exit",code});}};
 try{
  std::optional<std::string> message;
  if(input.contains("message")&&!input["message"].is_null())message=input["message"].get<std::string>();
  if(input.value("action","")=="error")cli::cliError(message,exits);
  else if(input.value("action","")=="ok")cli::cliOk(message,exits);
  else trace.push_back({"result",cli::ndjsonSafeStringify(input.at("value"),[&](const Json& value)->std::optional<std::string>{
   trace.push_back({"stringify",value});
   if(input.value("throw",false))throw std::runtime_error("stringify failed");
   if(input.value("undefined",false))return std::nullopt;
   return value::stringify(value);
  })});
 }catch(const std::exception& e){trace.push_back({"throw",e.what()});}
 std::cout<<trace.dump()<<'\n';
}}
