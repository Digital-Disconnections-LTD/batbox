#include "flights/ProjectDirectories.hpp"
#include "flights/Paths.hpp"
#include "gadget_results/Storage.hpp"
#include "runtime/PathSanitization.hpp"
#include "utils/hash.hpp"
#include <algorithm>
#include <iostream>
using namespace batbox;
Json evaluate(const Json& input){
  Json calls=Json::array(),values=Json::array();
  if(value::property(input,"op")=="order"){
    const auto cwd=[&]{calls.push_back("cwd");return "/work";};
    const auto project=[&](const std::string& cwd){calls.push_back("project:"+cwd);return "/projects/work";};
    const auto flight=[&]{calls.push_back("session");return "s";};
    std::string value;const auto method=input.at("method");
    if(method=="storage"){
      gadget_results::StorageImports imports;imports.getOriginalCwd=cwd;imports.getProjectDir=project;imports.getFlightId=flight;
      value=gadget_results::getFlightDir(imports);
    }else {
      flights::PathServices services;services.getOriginalCwd=cwd;services.getProjectDir=project;services.getFlightId=flight;
      services.getFlightProjectDir=[&]{calls.push_back("override");const auto& v=value::property(input,"override");return v.is_string()?std::optional<std::string>(v):std::nullopt;};
      flights::Paths paths(services);value=method=="agent"?paths.getBatTranscriptPath("kid"):paths.getTranscriptPath();
    }
    return {{"value",value},{"calls",calls}};
  }
  std::string home="/config";std::function<std::string(const std::string&)> hash;
  if(input.contains("bunHash"))hash=[&](const auto& name){
    calls.push_back("hash:"+name);auto value=std::stoull(input.at("bunHash").get<std::string>());std::string text;
    do{const auto digit=value%36;text+=static_cast<char>(digit<10?'0'+digit:'a'+digit-10);value/=36;}while(value);
    std::reverse(text.begin(),text.end());return text;
  };
  flights::ProjectDirectories dirs([&]{calls.push_back("config");return home;},hash);
  for(const auto& step:input.at("steps")){
    const auto op=step.at("op");
    if(op=="home"){home=step.at("value");values.push_back(nullptr);}
    else if(op=="hash")values.push_back(utils::djb2Hash(step.at("value").get<std::string>()));
    else if(op=="sanitize")values.push_back(runtime::sanitizePath(step.at("value"),hash));
    else if(op=="projects")values.push_back(dirs.getProjectsDir());
    else if(op=="portable")values.push_back(dirs.getProjectDirPortable(step.at("value")));
    else values.push_back(dirs.getProjectDir(step.at("value")));
  }
  return {{"value",values},{"calls",calls}};
}
int main(){std::string line;while(std::getline(std::cin,line))std::cout<<value::stringify(evaluate(value::parse(line)))<<'\n';}
