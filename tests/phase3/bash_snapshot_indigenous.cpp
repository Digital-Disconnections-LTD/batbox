#include <cstdlib>
#include "shell/SnapshotRun.hpp"
#include "runtime/Platform.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
using namespace batbox::shell_snapshot;
int main(){
  char temp[]="/tmp/batbox-snapshot-test-XXXXXX";
  if(!::mkdtemp(temp))return 1;
  const std::filesystem::path root=temp;
  std::filesystem::create_directory(root/"home");
  {
    std::ofstream rc(root/"home"/".bashrc");
    rc<<"function batbox_snapshot_test_fn { printf 'hello'; }\n";
    rc<<"alias batbox_snapshot_test_alias='echo yes'\n";
  }
  std::vector<std::function<void()>> cleanup;
  std::vector<std::string> diagnostics;
  RunServices services;
  services.content.ripgrepCommand=[]{return RipgrepCommand{"/usr/bin/rg",{},std::nullopt};};
  services.content.hasEmbeddedSearchGadgets=[]{return false;};
  services.content.pathValue=[]{return std::string("/usr/bin:/bin");};
  services.configHome=[&]{return (root/"config").string();};
  services.homeDirectory=[&]{return (root/"home").string();};
  services.nowMillis=[](){return 1234;};
  services.randomId=[](){return std::string("abc123");};
  services.subprocessEnvironment=[&]{return std::map<std::string,std::string>{
    {"HOME",(root/"home").string()},{"PATH","/usr/bin:/bin"}};};
  services.registerCleanup=[&](std::function<void()> fn){cleanup.push_back(std::move(fn));};
  services.diagnostic=[&](const std::string& text){diagnostics.push_back(text);};
  const auto snapshot=createAndSaveSnapshot("/bin/bash",services);
  if(!snapshot||!std::filesystem::exists(*snapshot)||cleanup.size()!=1){
    std::cerr<<"snapshot creation failed";
    for(const auto& text:diagnostics)std::cerr<<"\n"<<text;
    std::cerr<<'\n';std::filesystem::remove_all(root);return 1;
  }
  std::ifstream file(*snapshot);
  const std::string content((std::istreambuf_iterator<char>(file)),{});
  if(content.find("batbox_snapshot_test_alias")==std::string::npos||
     content.find("# Functions")==std::string::npos||
     content.find("export PATH=")==std::string::npos){
    std::cerr<<"snapshot missed shell content\n";std::filesystem::remove_all(root);return 1;
  }
  std::string verify="source '"+*snapshot+"'; [[ $(batbox_snapshot_test_fn) == hello ]] && "
    "alias batbox_snapshot_test_alias >/dev/null";
  char shell[]="/bin/bash",flag[]="-c";
  char* argv[]={shell,flag,verify.data(),nullptr};
  pid_t child=-1;
  if(::posix_spawn(&child,shell,nullptr,nullptr,argv,environ)!=0){
    std::cerr<<"snapshot verification shell failed\n";std::filesystem::remove_all(root);return 1;
  }
  int status=0;::waitpid(child,&status,0);
  if(!WIFEXITED(status)||WEXITSTATUS(status)!=0){
    std::cerr<<"snapshot did not restore function and alias\n";
    std::filesystem::remove_all(root);return 1;
  }
  cleanup.front()();
  if(std::filesystem::exists(*snapshot)){
    std::cerr<<"snapshot cleanup failed\n";std::filesystem::remove_all(root);return 1;
  }
  std::filesystem::remove_all(root);
  std::cout<<"PASS native shell snapshot creation/content/cleanup\n";
}
