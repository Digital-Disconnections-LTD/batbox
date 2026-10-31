#include "shell/BashProvider.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line))try{
  const auto input=batbox::value::parse(line);
  const auto shellPath=input.value("shellPath",std::string("/bin/bash"));
  int tmuxInitialized=0;
  batbox::shell_provider::Services services;
  services.skipSnapshot=input.value("skipSnapshot",false);
  services.createSnapshot=[input](const std::string&){
    std::promise<std::optional<std::string>> promise;
    if(input.value("snapshotError",false))promise.set_exception(
      std::make_exception_ptr(std::runtime_error("snapshot error")));
    else if(input.contains("snapshot")&&input["snapshot"].is_string())
      promise.set_value(input["snapshot"].get<std::string>());
    else promise.set_value(std::nullopt);
    return promise.get_future().share();
  };
  services.snapshotAccessible=[input](const std::string&){
    if(input.value("snapshotAccessError",false))throw std::runtime_error("missing snapshot");
    return input.value("snapshotAccessible",true);};
  services.operatingSystemTempDir=[input]{return input.value("tmpdir",std::string("/tmp"));};
  services.flightEnvironmentScript=[input]()->std::optional<std::string>{
    if(input.contains("sessionScript")&&input["sessionScript"].is_string())
      return input["sessionScript"].get<std::string>();
    return std::nullopt;
  };
  services.flightEnvVars=[input]{
    std::map<std::string,std::string> result;
    if(input.contains("sessionEnv"))for(auto it=input["sessionEnv"].begin();it!=input["sessionEnv"].end();++it)
      result[it.key()]=it.value().get<std::string>();
    return result;
  };
  services.tmuxGadgetUsed=[input]{return input.value("tmuxToolUsed",false);};
  services.ensureTmuxSocket=[&]{++tmuxInitialized;};
  services.tmuxEnvironment=[input]()->std::optional<std::string>{
    if(input.contains("tmuxEnv")&&input["tmuxEnv"].is_string())
      return input["tmuxEnv"].get<std::string>();
    return std::nullopt;
  };
  services.indigenousToPosixPath=[](const std::string& value){return value;};
  services.indigenousJoin=[](const std::string& a,const std::string& b){return a+"/"+b;};
  services.windows=input.value("windows",false);
  services.shellPrefix=input.value("shellPrefix",std::string());
  services.internalOperator=input.value("internalOperator",false);
  batbox::shell_provider::BashProvider provider(shellPath,services);
  batbox::shell_provider::BuildOptions options;
  options.id=input.value("id",std::string("a123"));
  if(input.contains("sandboxTmpDir")&&input["sandboxTmpDir"].is_string())
    options.sandboxTmpDir=input["sandboxTmpDir"].get<std::string>();
  options.useSandbox=input.value("useSandbox",false);
  const auto command=input.at("command").get<std::string>();
  const auto built=provider.buildExecCommand(command,options);
  const auto spawnArgs=provider.getSpawnArgs(built.commandString);
  const auto environment=provider.getEnvironmentOverrides(command);
  Json result={{"commandString",built.commandString},{"cwdFilePath",built.cwdFilePath},
    {"spawnArgs",spawnArgs},{"environment",environment},{"tmuxInitialized",tmuxInitialized}};
  std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
}catch(const std::exception& error){
  std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
}}
