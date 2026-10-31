#include "mcp/ProcessResult.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    batbox::mcp::ProcessResultServices services;
    services.result.resizeImage=[](const std::string& bytes,std::size_t,const std::string&){
      return batbox::mcp::ResizedImage{bytes,"png"};};
    services.result.persistBinaryContent=[](const std::string& bytes,
        const std::optional<std::string>&,const std::string& id){
      return batbox::Json{{"filepath","/tmp/output/"+id+".bin"},{"size",bytes.size()}};};
    services.result.nowMs=[](){return std::int64_t{1700000000000};};
    services.result.randomSuffix=[](){return "i";};
    services.result.hostedServerPrefix="claude.ai ";
    if(input.contains("env")&&input.at("env").is_string())
      services.validation.envMaxOutputTokens=input.at("env").get<std::string>();
    if(input.contains("configured")&&input.at("configured").is_number())
      services.validation.configuredMaxOutputTokens=input.at("configured").get<double>();
    services.validation.countMessagesTokensWithAPI=[input](const batbox::Json&,const batbox::Json&){
      return std::optional<double>(input.value("count",0.0));};
    if(input.contains("largeFiles")&&input.at("largeFiles").is_string())
      services.enableLargeOutputFiles=input.at("largeFiles").get<std::string>();
    services.nowMs=[](){return std::int64_t{1700000000000};};
    services.persistGadgetResult=[input](const std::string& content,const std::string& id){
      if(input.value("failPersist",false))return batbox::Json{{"error","disk failed"}};
      return batbox::Json{{"filepath","/tmp/output/"+id+".txt"},
        {"originalSize",batbox::value::stringLength(content)}};};
    const auto result=batbox::mcp::processMCPResult(input.at("value"),
      input.value("tool",std::string("test")),
      input.value("server",std::string("demo")),services);
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
