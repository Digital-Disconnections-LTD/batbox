#include <cstdlib>
#include "shell/ManagedShell.hpp"
#include "shell/BashProvider.hpp"
#include "shell/ShellCwdCompletion.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sys/stat.h>
#include <stop_token>
#include <unistd.h>
int main(){
  char temp[]="/tmp/batbox-shell-exec-XXXXXX";if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=std::filesystem::canonical(temp);
  try{
    batbox::shell::FlightConfig config;config.shellPath="/bin/bash";
    config.originalCwd=root.string();config.outputDir=(root/"jobs").string();
    config.generateMissionId=[](){return std::string("task1");};
    config.splitCommand=[](const std::string&){return std::vector<std::string>{"echo"};};
    batbox::shell::ManagedShell shell(config);
    std::string built;std::vector<std::string> diagnostics;
    batbox::shell::RunOptions options;options.command="echo sandbox";
    options.timeoutMs=2000;options.useSandbox=true;
    options.sandboxTmpDir=(root/"sandbox").string();
    options.sandboxWrap=[&](const std::string& command){built=command;return command;};
    options.logDebug=[&](const std::string& message){diagnostics.push_back(message);};
    const auto result=shell.run(options);
    if(result.code!=0||result.stdoutText.find("sandbox")==std::string::npos||
      built.find("echo sandbox")==std::string::npos||!diagnostics.empty())
      throw std::runtime_error("sandbox preparation order failed");
    struct stat info{};
    if(::stat(options.sandboxTmpDir->c_str(),&info)!=0||(info.st_mode&0777)!=0700)
      throw std::runtime_error("sandbox temp directory mode wrong");
    const auto vanished=root/"vanished";std::filesystem::create_directory(vanished);
    shell.setCwd(vanished.string());std::filesystem::remove(vanished);
    const auto recovered=shell.run(batbox::shell::RunOptions{.command="pwd"});
    if(recovered.code!=0||shell.cwd()!=root.string())
      throw std::runtime_error("deleted cwd did not recover before spawn");
    std::stop_source stopped;stopped.request_stop();
    const auto aborted=shell.run(batbox::shell::RunOptions{.command="echo must-not-run",
      .signal=stopped.get_token()});
    if(aborted.code!=145||!aborted.interrupted)
      throw std::runtime_error("pre-spawn abort result wrong");
    std::vector<std::string> order;
    batbox::shell_provider::Services providerServices;providerServices.skipSnapshot=true;
    providerServices.flightEnvironmentScript=[](){return std::optional<std::string>{};};
    providerServices.flightEnvVars=[&](){order.push_back("env");
      return std::map<std::string,std::string>{};};
    providerServices.tmuxGadgetUsed=[](){return false;};
    providerServices.ensureTmuxSocket=[](){};
    providerServices.tmuxEnvironment=[](){return std::optional<std::string>{};};
    providerServices.operatingSystemTempDir=[&](){order.push_back("build");return root.string();};
    providerServices.snapshotAccessible=[](const std::string&){return true;};
    config.provider=std::make_shared<batbox::shell_provider::BashProvider>("/bin/bash",
      std::move(providerServices));
    config.generateExecId=[](){return std::string("abcd");};
    batbox::shell::ManagedShell providerShell(config);
    auto providerOptions=options;providerOptions.command="echo provider sandbox";
    providerOptions.sandboxTmpDir=(root/"provider-sandbox").string();
    providerOptions.sandboxWrap=[&](const std::string& command){
      order.push_back("wrap");return command;};
    const auto providerResult=providerShell.run(providerOptions);
    if(providerResult.code!=0||providerResult.stdoutText.find("provider sandbox")==std::string::npos||
      order!=std::vector<std::string>{"build","wrap","env"})
      throw std::runtime_error("provider/sandbox/environment ordering wrong");
    auto lifecycle=std::make_shared<batbox::shell::CwdCompletionServices>();
    std::weak_ptr<batbox::shell::ManagedShell> weak;
    int invalidations=0,hooks=0,unlinkCalls=0,failures=0;
    lifecycle->readFileUtf8=[](const std::string& path){
      std::ifstream file(path);return std::string(std::istreambuf_iterator<char>{file},{});};
    lifecycle->normalizeNFC=[](const std::string& text){return text;};
    lifecycle->setCwd=[&](const std::string& next,const std::string& previous){
      if(previous!=root.string())throw std::runtime_error("old cwd wrong");
      if(auto current=weak.lock())current->setCwd(next);};
    lifecycle->invalidateFlightEnvStash=[&]{++invalidations;};
    lifecycle->onCwdChangedForHooks=[&](const std::string&,const std::string&){++hooks;};
    lifecycle->unlinkFile=[&](const std::string& path){++unlinkCalls;
      std::filesystem::remove(path);};
    lifecycle->onFailure=[&](std::exception_ptr){++failures;};
    config.cwdCompletion=lifecycle;
    auto strictShell=std::make_shared<batbox::shell::ManagedShell>(config);weak=strictShell;
    const auto cwdResult=strictShell->run(batbox::shell::RunOptions{.command="cd /tmp"});
    if(cwdResult.code!=0||strictShell->cwd()!=std::filesystem::canonical("/tmp").string()||invalidations!=1||hooks!=1||
       unlinkCalls!=1||failures!=0)
      throw std::runtime_error("cwd completion was not bound before result publication");
    invalidations=hooks=unlinkCalls=failures=0;
    auto backgroundShell=std::make_shared<batbox::shell::ManagedShell>(config);weak=backgroundShell;
    const auto background=backgroundShell->run(batbox::shell::RunOptions{
      .command="sleep 0.1; cd /tmp",.runInBackground=true});
    if(!background.backgroundMissionId)
      throw std::runtime_error("background cwd fixture did not start");
    const auto finished=backgroundShell->wait(*background.backgroundMissionId,2000);
    if(finished.code!=0||backgroundShell->cwd()!=root.string()||invalidations!=0||hooks!=0||
       unlinkCalls!=1||failures!=0)
      throw std::runtime_error("background cwd cleanup happened after result or changed session cwd");
    backgroundShell->cleanup(*background.backgroundMissionId);
    std::filesystem::remove_all(root);
    std::cout<<"PASS Shell.exec sandbox wrap after build and 0700 temp directory\n";return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';
    std::filesystem::remove_all(root);return 1;}
}
