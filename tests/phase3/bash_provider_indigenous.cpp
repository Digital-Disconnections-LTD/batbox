#include <cstdlib>
#include "shell/BashProvider.hpp"
#include "shell/ManagedShell.hpp"
#include "shell/SnapshotRun.hpp"
#include "shell/CommandSplitter.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <unistd.h>
using namespace batbox;
int main(){
  char temp[]="/tmp/batbox-provider-test-XXXXXX";
  if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=std::filesystem::canonical(temp);
  std::filesystem::create_directories(root/"home");
  std::filesystem::create_directories(root/"tmp");
  std::filesystem::create_directories(root/"work"/"sub");
  {std::ofstream rc(root/"home"/".bashrc");
    rc<<"function batbox_provider_fn { printf 'snapshot-ok'; }\n";}
  std::vector<std::function<void()>> cleanup;
  std::vector<std::string> diagnostics;
  shell_snapshot::RunServices snapshot;
  snapshot.content.ripgrepCommand=[]{return shell_snapshot::RipgrepCommand{"/usr/bin/rg",{},std::nullopt};};
  snapshot.content.hasEmbeddedSearchGadgets=[]{return false;};
  snapshot.content.pathValue=[]{return std::string("/usr/bin:/bin");};
  snapshot.configHome=[&]{return (root/"config").string();};
  snapshot.homeDirectory=[&]{return (root/"home").string();};
  snapshot.nowMillis=[](){return 1234;};
  snapshot.randomId=[](){return std::string("provider1");};
  snapshot.subprocessEnvironment=[&]{return std::map<std::string,std::string>{
    {"HOME",(root/"home").string()},{"PATH","/usr/bin:/bin"}};};
  snapshot.registerCleanup=[&](std::function<void()> fn){cleanup.push_back(std::move(fn));};
  snapshot.diagnostic=[&](const std::string& text){diagnostics.push_back(text);};
  shell_provider::Services services;
  services.createSnapshot=[snapshot](const std::string& path){
    return std::async(std::launch::async,[snapshot,path]{
      return shell_snapshot::createAndSaveSnapshot(path,snapshot);}).share();};
  services.flightEnvironmentScript=[]()->std::optional<std::string>{
    return "export BATBOX_SESSION_FLAG=yes";};
  services.flightEnvVars=[]{return std::map<std::string,std::string>{
    {"BATBOX_SESSION_VAR","present"}};};
  services.tmuxGadgetUsed=[]{return false;};
  services.ensureTmuxSocket=[]{};
  services.tmuxEnvironment=[]()->std::optional<std::string>{return std::nullopt;};
  services.operatingSystemTempDir=[&]{return (root/"tmp").string();};
  services.snapshotAccessible=[](const std::string& path){return std::filesystem::exists(path);};
  auto provider=std::make_shared<shell_provider::BashProvider>("/bin/bash",services);
  std::atomic<int> ids{0};
  shell::FlightConfig config;
  config.shellPath="/bin/bash";config.provider=provider;
  config.originalCwd=(root/"work").string();config.outputDir=(root/"jobs").string();
  config.generateMissionId=[&]{return "task"+std::to_string(++ids);};
  config.generateExecId=[&]{return "exec"+std::to_string(ids.load());};
  config.splitCommand=[](const std::string& value){return shell_split::splitCommandDeprecated(value);};
  config.environmentOverrides={{"HOME",(root/"home").string()},{"PATH","/usr/bin:/bin"}};
  shell::ManagedShell shell(config);
  const auto output=shell.run({.command="batbox_provider_fn; printf ':%s:%s' \"$BATBOX_SESSION_FLAG\" \"$BATBOX_SESSION_VAR\""});
  if(output.code!=0||output.stdoutText!="snapshot-ok:yes:present"){
    std::cerr<<"provider/snapshot execution failed: "<<output.toJson().dump()<<'\n';
    for(const auto& text:diagnostics)std::cerr<<text<<'\n';
    std::filesystem::remove_all(root);return 1;
  }
  const auto moved=shell.run({.command="cd sub"});
  if(moved.code!=0||shell.cwd()!=(root/"work"/"sub").string()){
    std::cerr<<"provider cwd update failed\n";std::filesystem::remove_all(root);return 1;}
  if(cleanup.size()!=1){std::cerr<<"snapshot cleanup missing\n";std::filesystem::remove_all(root);return 1;}
  for(auto& fn:cleanup)fn();
  std::filesystem::remove_all(root);
  std::cout<<"PASS provider snapshot/session environment/native shell/cwd\n";
}
