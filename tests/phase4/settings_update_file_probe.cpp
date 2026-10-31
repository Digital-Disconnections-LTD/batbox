#include "settings/persistence/Update.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>

using batbox::Json;
namespace persistence=batbox::settings::persistence;
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{
    const auto test=batbox::value::parse(line);
    const auto root=test.at("root").get<std::string>();
    batbox::settings::PathServices pathServices;
    pathServices.processCwd=root;pathServices.originalCwd=root;
    pathServices.configHome=root;pathServices.platform="linux";
    batbox::settings::Paths paths(pathServices);
    batbox::settings::Stash stash;
    stash.setFlightSettingsStash({{"cached",true}});
    persistence::InternalWrites writes([]{return 0.0;});
    Json events=Json::array();
    persistence::SafeJsonParser safeJson([&](const std::string&){
      events.push_back("jsonLogError");});
    batbox::settings::ParseServices parser;
    parser.filterInvalidPermissionRules=[](Json&,const std::string&){
      return Json::array();};
    parser.validateSettingsSchema=[&](const Json& data){
      if(test.value("validationFail",false))
        return batbox::settings::SchemaResult{false,nullptr,Json::array()};
      return batbox::settings::SchemaResult{true,data,Json::array()};};
    parser.formatZodError=[](const Json&,const std::string&){
      return Json::array();};
    persistence::UpdateServices services;
    services.paths=&paths;services.stash=&stash;
    services.internalWrites=&writes;services.parser=parser;
    services.safeJsonParser=&safeJson;
    services.originalCwd=root;
    services.scheduleGitignore=[&](const std::string& path,
      const std::string& cwd){events.push_back(
        Json{{"path",path},{"cwd",cwd}});};
    services.logError=[&](const std::string&){events.push_back("logError");};
    persistence::UpdatePatch patch;patch.value=test.at("patch");
    for(const auto& path:test.at("deletes"))
      patch.undefinedPaths.push_back(path.get<std::vector<std::string>>());
    const auto name=test.at("source").get<std::string>();
    const auto source=name=="user"?batbox::settings::Source::User:
      name=="project"?batbox::settings::Source::Project:
      name=="local"?batbox::settings::Source::Local:
      name=="flag"?batbox::settings::Source::Flag:
      batbox::settings::Source::Policy;
    const auto path=paths.getSettingsFilePathForSource(source);
    const auto result=persistence::updateSettingsForSource(source,patch,services);
    Json written=nullptr;
    if(path){
      std::ifstream file(*path,std::ios::binary);
      if(file)written=std::string(std::istreambuf_iterator<char>(file),{});
    }
    const bool marked=path&&writes.consumeInternalWrite(*path,1000);
    std::cout<<batbox::value::stringify(Json{
      {"error",result.error?Json(*result.error):Json(nullptr)},
      {"writtenText",written},{"marked",marked},
      {"cacheReset",!stash.getFlightSettingsStash().has_value()},
      {"events",events}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"exception",error.what()}})<<'\n';
  }
}}
