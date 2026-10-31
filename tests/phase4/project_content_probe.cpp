#include "instructions/project/ProjectMemory.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace project=batbox::instructions::project;
namespace value=batbox::value;
namespace {
std::string value(const Json& object,const char* key){
  const auto& raw=value::property(object,key);
  return raw.is_string()?raw.get<std::string>():std::string{};
}
Json run(const Json& input){
  const auto content=value(input,"content");
  if(value::property(input,"op")=="frontmatter"){
    const auto parsed=project::parseFrontmatterPaths(content);
    Json result={{"content",parsed.content}};
    if(parsed.paths){result["paths"]=Json::array();
      for(const auto& item:*parsed.paths)result["paths"].push_back(item);}
    return {{"result",result}};
  }
  project::ParseServices services;
  services.paths.getCwd=[]{return std::string("/project");};
  services.paths.homeDirectory=[]{return std::string("/home/user");};
  services.paths.platform=[]{return std::string("linux");};
  services.paths.normalizeNFC=[](const std::string& text){return text;};
  std::optional<std::string> include;
  if(input.contains("includeBasePath")&&input.at("includeBasePath").is_string())
    include=value(input,"includeBasePath");
  const auto parsed=project::parseMemoryFileContent(content,value(input,"path"),
    value(input,"type"),include,services);
  Json paths=Json::array();
  for(const auto& item:parsed.includePaths)paths.push_back(item);
  return {{"result",Json{{"info",parsed.info
    ?project::memoryFileToJson(*parsed.info):Json(nullptr)},
    {"includePaths",paths}}}};
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
