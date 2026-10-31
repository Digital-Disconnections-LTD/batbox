#include "instructions/MemoryPath.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <iostream>

int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    batbox::instructions::MemoryPathServices services;
    const auto& sourceEnv=batbox::value::property(input,"env");
    for(const auto& [field,key]:std::initializer_list<std::pair<const char*,std::string>>{
      {"disable",services.keys.disableAutoMemory},{"simple",services.keys.simpleMode},
      {"remote",services.keys.remoteMode},{"remoteDir",services.keys.remoteMemoryDir},
      {"override",services.keys.fullPathOverride}}){
      if(sourceEnv.is_object()&&sourceEnv.contains(field))services.environment[key]=sourceEnv.at(field);
    }
    services.configHomeDir=input.value("configHome",std::string("/home/test/.batbox"));
    services.homeDirectory=input.value("home",std::string("/home/test"));
    services.projectRoot=input.value("projectRoot",std::string("/work/project"));
    services.initialSettings=input.value("initialSettings",batbox::Json::object());
    services.settingsBySource=input.value("settings",batbox::Json::object());
    services.findCanonicalGitRoot=[input](const std::string&)->std::optional<std::string>{
      const auto& root=batbox::value::property(input,"gitRoot");
      return root.is_string()?std::optional<std::string>(root.get<std::string>()):std::nullopt;};
    batbox::instructions::MemoryPathStash stash;
    const auto op=input.at("op").get<std::string>();
    batbox::Json result;
    if(op=="enabled")result=batbox::instructions::isAutoMemoryEnabled(services);
    else if(op=="base")result=batbox::instructions::getMemoryBaseDir(services);
    else if(op=="validate"){
      const auto& raw=batbox::value::property(input,"raw");
      const auto value=batbox::instructions::validateMemoryPath(raw.is_string()
        ?std::optional<std::string>(raw.get<std::string>()):std::nullopt,
        input.value("expandTilde",false),services.homeDirectory);
      if(value)result=*value;
    }else if(op=="override")result=batbox::instructions::hasAutoMemPathOverride(services);
    else if(op=="sanitize")result=batbox::instructions::sanitizeMemoryProjectPath(
      input.at("raw").get<std::string>());
    else if(op=="entrypoint")result=batbox::instructions::getAutoMemEntrypoint(services,stash);
    else if(op=="inside")result=batbox::instructions::isAutoMemPath(
      input.at("path").get<std::string>(),services,stash);
    else if(op=="daily")result=batbox::instructions::getAutoMemDailyLogPath(services,stash,
      std::chrono::system_clock::time_point(
        std::chrono::milliseconds(input.at("dateMs").get<std::int64_t>())));
    else if(op=="memo"){
      result=batbox::Json::array();
      result.push_back(batbox::instructions::getAutoMemPath(services,stash));
      services.environment[services.keys.fullPathOverride]=input.at("changedOverride");
      result.push_back(batbox::instructions::getAutoMemPath(services,stash));
    }else result=batbox::instructions::getAutoMemPath(services,stash);
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
