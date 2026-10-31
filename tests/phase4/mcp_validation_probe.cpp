#include "mcp/Validation.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    batbox::mcp::ValidationServices services;
    if(input.contains("env")&&input.at("env").is_string())
      services.envMaxOutputTokens=input.at("env").get<std::string>();
    if(input.contains("configured")&&input.at("configured").is_number())
      services.configuredMaxOutputTokens=input.at("configured").get<double>();
    services.countMessagesTokensWithAPI=[input](const batbox::Json&,const batbox::Json&){
      if(input.value("countError",false))throw std::runtime_error("count failed");
      if(input.contains("count")&&input.at("count").is_number())
        return std::optional<double>(input.at("count").get<double>());
      return std::optional<double>();};
    services.compressImageBlock=[input](const batbox::Json& block,std::size_t bytes){
      if(input.value("compressError",false))throw std::runtime_error("compress failed");
      auto output=block;
      output["source"]={{"type","base64"},{"data",std::string(bytes,'A')},
        {"media_type","image/png"}};
      return output;};
    const auto op=input.at("op").get<std::string>();
    const auto content=input.value("content",batbox::Json());
    batbox::Json result;
    if(op=="max")result=batbox::mcp::getMaxMcpOutputTokens(services);
    else if(op=="estimate")result=batbox::mcp::getContentSizeEstimate(content);
    else if(op=="needs")result=batbox::mcp::mcpContentNeedsTruncation(content,services);
    else if(op=="truncate")result=batbox::mcp::truncateMcpContent(content,services);
    else result=batbox::mcp::truncateMcpContentIfNeeded(content,services);
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
