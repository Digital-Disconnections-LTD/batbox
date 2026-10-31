#include "mcp/Mcp.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    batbox::mcp::ResultServices services;
    services.hostedServerPrefix=input.value("hostedPrefix",std::string("claude.ai "));
    services.nowMs=[](){return 1700000000000LL;};
    services.randomSuffix=[](){return std::string("i");};
    services.resizeImage=[](const std::string& bytes,std::size_t,const std::string&){
      return batbox::mcp::ResizedImage{bytes,"png"};};
    const bool failPersist=input.value("failPersist",false);
    services.persistBinaryContent=[failPersist](const std::string& bytes,
      const std::optional<std::string>&,const std::string& id)->Json{
      if(failPersist)return {{"error","disk failed"}};
      return {{"filepath","/tmp/output/"+id+".bin"},{"size",bytes.size()},{"ext","bin"}};};
    Json result;
    const auto server=input.value("server",std::string("demo"));
    if(input.at("op")=="content")
      result=batbox::mcp::transformResultContent(input.at("value"),server,services);
    else if(input.at("op")=="result"){
      const auto output=batbox::mcp::transformMCPResult(input.at("value"),
        input.value("tool",std::string("test")),server,services);
      result={{"content",output.content},{"type",output.type}};
      if(!output.schema.empty())result["schema"]=output.schema;
    }else throw std::runtime_error("unknown operation");
    std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
  }
}
