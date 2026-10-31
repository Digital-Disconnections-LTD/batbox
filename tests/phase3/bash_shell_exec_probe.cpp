#include <cstdlib>
#include "shell/ManagedShell.hpp"
#include "shell/BashProvider.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <iostream>
#include <stop_token>
#include <unistd.h>
int main(){
  char temp[]="/tmp/batbox-shell-exec-probe-XXXXXX";if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=temp;
  try{
    batbox::Json output=batbox::Json::array();
    for(const bool missing:{false,true}){
      std::vector<std::string> events;
      batbox::shell_provider::Services providerServices;
      providerServices.skipSnapshot=true;
      providerServices.flightEnvironmentScript=[](){return std::optional<std::string>{};};
      providerServices.flightEnvVars=[](){return std::map<std::string,std::string>{};};
      providerServices.tmuxGadgetUsed=[](){return false;};
      providerServices.ensureTmuxSocket=[](){};
      providerServices.tmuxEnvironment=[](){return std::optional<std::string>{};};
      providerServices.operatingSystemTempDir=[&](){events.push_back("build");return root.string();};
      providerServices.snapshotAccessible=[](const std::string&){return true;};
      auto provider=std::make_shared<batbox::shell_provider::BashProvider>("/bin/bash",
        std::move(providerServices));
      batbox::shell::FlightConfig config;config.shellPath="/bin/bash";
      config.originalCwd=missing?(root/"missing").string():root.string();
      config.outputDir=(root/"jobs").string();config.provider=provider;
      config.generateMissionId=[](){return std::string("task1");};
      config.generateExecId=[](){return std::string("8000");};
      config.splitCommand=[](const std::string&){return std::vector<std::string>{"echo"};};
      batbox::shell::ManagedShell shell(config);
      std::stop_source stop;stop.request_stop();
      auto result=shell.run(batbox::shell::RunOptions{.command="echo hi",.signal=stop.get_token()});
      output.push_back({{"value",{{"kind",missing?"failed":"aborted"},
        {"code",result.code},{"stderr",result.stderrText},
        {"interrupted",result.interrupted}}},{"events",events}});
    }
    std::vector<std::string> events;
    batbox::shell_provider::Services providerServices;providerServices.skipSnapshot=true;
    providerServices.flightEnvironmentScript=[](){return std::optional<std::string>{};};
    providerServices.flightEnvVars=[&](){events.push_back("env");
      return std::map<std::string,std::string>{};};
    providerServices.tmuxGadgetUsed=[](){return false;};
    providerServices.ensureTmuxSocket=[](){};
    providerServices.tmuxEnvironment=[](){return std::optional<std::string>{};};
    providerServices.operatingSystemTempDir=[&](){events.push_back("build");return root.string();};
    providerServices.snapshotAccessible=[](const std::string&){return true;};
    batbox::shell::FlightConfig config;config.shellPath="/bin/bash";config.originalCwd=root.string();
    config.outputDir=(root/"jobs").string();config.generateMissionId=[](){return std::string("task1");};
    config.generateExecId=[](){return std::string("8000");};
    config.splitCommand=[](const std::string&){return std::vector<std::string>{"echo"};};
    config.provider=std::make_shared<batbox::shell_provider::BashProvider>("/bin/bash",
      std::move(providerServices));
    batbox::shell::ManagedShell shell(config);
    batbox::shell::RunOptions options;options.command="echo provider sandbox";
    options.useSandbox=true;options.sandboxTmpDir=(root/"sandbox").string();
    options.sandboxWrap=[&](const std::string& command){events.push_back("wrap");return command;};
    options.logDebug=[](const std::string&){};
    const auto result=shell.run(options);
    if(result.code!=0)throw std::runtime_error("native sandbox command failed");
    output.push_back({{"value",{{"kind","started"}}},{"events",events}});
    std::cout<<batbox::value::stringify(output)<<'\n';std::filesystem::remove_all(root);return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';
    std::filesystem::remove_all(root);return 1;}
}
