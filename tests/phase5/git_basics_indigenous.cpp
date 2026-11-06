#include "git/CanonicalRoot.hpp"
#include "git/GitBasics.hpp"
#include "git/GitBindings.hpp"
#include "git_filesystem/GitFilesystem.hpp"
#include "git_filesystem/RootFinder.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

using namespace batbox;
namespace fs=std::filesystem;
namespace {
void gitCommand(const std::vector<std::string>& args){
  std::vector<char*> argv;argv.push_back(const_cast<char*>("git"));
  for(const auto& arg:args)argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);
  const auto pid=::fork();assert(pid>=0);
  if(pid==0){::execvp("git",argv.data());::_exit(127);}
  int status=0;assert(::waitpid(pid,&status,0)==pid);
  assert(WIFEXITED(status)&&WEXITSTATUS(status)==0);
}
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> promise){
  std::optional<T> result;promise.then([&](const T* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);result=*value;});
  loop.run();assert(result);return *result;
}
}
int main(){
  char raw[]="/tmp/batbox-git-basics-XXXXXX";assert(::mkdtemp(raw));
  const fs::path temp=fs::canonical(raw),root=temp/"main",work=temp/"work";
  const auto oldCwd=fs::current_path();
  try{
    fs::create_directory(root);
    gitCommand({"-C",root.string(),"init","-q","-b","main"});
    gitCommand({"-C",root.string(),"config","user.email","test@invalid.test"});
    gitCommand({"-C",root.string(),"config","user.name","Test"});
    std::ofstream(root/"README")<<"test\n";
    gitCommand({"-C",root.string(),"add","README"});
    gitCommand({"-C",root.string(),"commit","-qm","first"});
    gitCommand({"-C",root.string(),"worktree","add","-q","-b","work",work.string()});
    auto finder=git_filesystem::indigenousRootFinder();
    auto canonicalImports=git::indigenousCanonicalRootImports();
    canonicalImports.findGitRoot=[finder](const std::string& path){return finder->find(path);};
    git::CanonicalRoot canonical(canonicalImports);
    assert(canonical.find((root/"README").string())==root.string());
    assert(canonical.find((work/"README").string())==root.string());
    int reads=0;
    auto stashImports=git::indigenousCanonicalRootImports();
    stashImports.findGitRoot=[](const std::string& path){return std::optional<std::string>(path);};
    stashImports.readFileSync=[&reads](const std::string&)->std::string{
      ++reads;throw std::runtime_error("ENOENT");};
    git::CanonicalRoot lru(stashImports);
    for(int index=0;index<50;++index)lru.resolve("/repo-"+std::to_string(index));
    assert(reads==50);
    assert(lru.resolve("/repo-49")=="/repo-49"&&reads==50);
    lru.resolve("/repo-50");
    assert(reads==51);
    lru.resolve("/repo-1");
    assert(reads==51);
    lru.resolve("/repo-0");
    assert(reads==52);

    event_loop::Loop loop;std::string cwd=work.string();
    auto services=git_filesystem::indigenousGitFsServices(loop);
    services.findGitRoot=[finder](const std::string& path){return finder->find(path);};
    services.getCwd=[&]{return cwd;};
    services.registerCleanup=[](std::function<void()>){ };
    git_filesystem::GitFilesystem filesystem(loop,services);
    Json gitDiagnostics=Json::array();
    auto imports=git::indigenousBasicsImports(finder,filesystem,[&]{return cwd;},
      [&](const std::string& level,const std::string& event,
        const std::optional<Json>& payload){
        gitDiagnostics.push_back({{"level",level},{"event",event},
          {"payload",payload?*payload:Json(nullptr)}});
      });
    git::GitBasics basics(loop,imports);
    context::AsyncServices contextServices;
    git::bindGitContextServices(contextServices,basics);
    assert(await(loop,basics.getIsGit()));
    assert(await(loop,contextServices.getIsGit()));
    assert(gitDiagnostics.size()==2);
    assert(gitDiagnostics[0]["event"]=="is_git_check_started");
    assert(gitDiagnostics[1]["event"]=="is_git_check_completed");
    assert(gitDiagnostics[1]["payload"]["is_git"]==true);
    assert(await(loop,basics.dirIsInGitRepo(work.string())));
    assert(await(loop,basics.getBranch())=="work");
    assert(await(loop,contextServices.getBranch())=="work");
    assert(!basics.gitExe().empty());
    assert(!await(loop,basics.dirIsInGitRepo((temp/"outside").string())));
    fs::remove_all(temp);
    std::cout<<"real Git root, worktree and owner-loop callbacks passed\n";
  }catch(...){fs::current_path(oldCwd);fs::remove_all(temp);throw;}
}
