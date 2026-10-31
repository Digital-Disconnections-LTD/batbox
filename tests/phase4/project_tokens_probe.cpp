#include "instructions/project/ProjectMemory.hpp"
#include "instructions/project/MarkdownLexer.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace project=batbox::instructions::project;
namespace value=batbox::value;
namespace {
std::string value(const Json& object,const char* key,const char* fallback=""){
  const auto& raw=value::property(object,key);
  return raw.is_string()?raw.get<std::string>():std::string(fallback);
}
Json run(const Json& input){
  const auto content=value(input,"content");
  if(value::property(input,"op")=="strip"){
    const auto result=project::stripHtmlComments(content);
    return {{"result",Json{{"content",result.content},{"stripped",result.stripped}}}};
  }
  project::ParseServices services;
  services.paths.getCwd=[]{return std::string("/project");};
  services.paths.homeDirectory=[]{return std::string("/home/user");};
  services.paths.platform=[]{return std::string("linux");};
  services.paths.normalizeNFC=[](const std::string& text){return text;};
  const auto tokens=project::lexMarkdown(content);
  const auto result=project::extractIncludePathsFromTokens(tokens,
    value(input,"basePath","/project/BATBOX.md"),services);
  Json output=Json::array();for(const auto& item:result)output.push_back(item);
  return {{"result",output}};
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
    catch(const std::exception& error)
      {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
  }
}
