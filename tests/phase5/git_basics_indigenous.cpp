#include "git/CanonicalRoot.hpp"
#include "git/BareRepository.hpp"
#include "git/GitBasics.hpp"
#include "git/GitBindings.hpp"
#include "git/GitCommands.hpp"
#include "git_filesystem/ConfigParser.hpp"
#include "git_filesystem/GitFilesystem.hpp"
#include "git_filesystem/RootFinder.hpp"
#include "process/ExecFileNoThrow.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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
    assert(canonical.stashSize()==2);
    auto stashImports=git::indigenousCanonicalRootImports();
    stashImports.findGitRoot=[](const std::string& path){return std::optional<std::string>(path);};
    git::CanonicalRoot lru(stashImports);
    for(int index=0;index<50;++index)lru.resolve("/repo-"+std::to_string(index));
    assert(lru.stashSize()==50);
    assert(lru.stashGet("/repo-0")=="/repo-0"); // peek does not change recency
    lru.resolve("/repo-50");
    assert(!lru.stashHas("/repo-0")&&lru.stashHas("/repo-1"));
    assert(lru.stashDelete("/repo-1")&&!lru.stashDelete("/repo-1"));
    lru.clear();assert(lru.stashSize()==0);
    auto bare=git::indigenousBareRepositoryImports([&]{return root.string();});
    assert(!git::isCurrentDirectoryBareGitRepo(bare));
    const auto broken=temp/"broken";fs::create_directory(broken);
    fs::create_directory(broken/".git");
    fs::create_directory(broken/"objects");
    bare.getCwd=[&]{return broken.string();};
    assert(git::isCurrentDirectoryBareGitRepo(bare));

    event_loop::Loop loop;std::string cwd=work.string();
    auto services=git_filesystem::indigenousGitFsServices(loop);
    services.findGitRoot=[finder](const std::string& path){return finder->find(path);};
    services.getCwd=[&]{return cwd;};
    git_filesystem::bindIndigenousConfigParser(loop,services);
    services.waitForScrollIdle=[&loop]{event_loop::Deferred<event_loop::Unit> value(loop);
      value.resolve(event_loop::Unit{});return value;};
    services.registerCleanup=[](std::function<void()>){ };
    git_filesystem::GitFilesystem filesystem(loop,services);
    Json gitDiagnostics=Json::array();
    auto imports=git::indigenousBasicsImports(loop,finder,filesystem,[&]{return cwd;},
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
    assert(await(loop,basics.isAtGitRoot()));
    assert(await(loop,basics.dirIsInGitRepo(work.string())));
    const auto gitdir=await(loop,basics.getGitDir(work.string()));
    assert(gitdir&&fs::exists(*gitdir));
    assert(await(loop,basics.getBranch())=="work");
    assert(await(loop,contextServices.getBranch())=="work");
    assert(await(loop,basics.getHead()).size()==40);
    assert(await(loop,basics.getWorktreeCount())==2);
    assert(!basics.gitExe().empty());
    cwd=(work/"missing").string();
    assert(!await(loop,basics.isAtGitRoot()));
    fs::current_path(work);
    process::ExecImports execImports;
    execImports.execa=process::indigenousExeca(loop);
    execImports.getCwd=[&]{return cwd;};
    execImports.logError=[](std::exception_ptr){};
    git::CommandImports commandsImports;
    commandsImports.gitExe=[&]{return basics.gitExe();};
    commandsImports.execFileNoThrow=[&](const std::string& file,
      const std::vector<std::string>& args,const process::ExecOptions& options){
      return process::execFileNoThrow(loop,file,args,options,execImports);};
    git::GitCommands commands(loop,commandsImports);
    assert(await(loop,commands.getIsClean()));
    std::ofstream(work/"new.txt")<<"new\n";
    assert(!await(loop,commands.getIsClean()));
    assert(await(loop,commands.getIsClean(true)));
    assert(await(loop,commands.getChangedFiles())==std::vector<std::string>{"new.txt"});
    assert(await(loop,commands.getFileStatus()).untracked==
      std::vector<std::string>{"new.txt"});
    assert(!await(loop,commands.getIsHeadOnRemote()));
    assert(!await(loop,commands.hasUnpushedCommits()));
    fs::current_path(oldCwd);
    fs::remove_all(temp);
    std::cout<<"real Git root, worktree and owner-loop callbacks passed\n";
  }catch(...){fs::current_path(oldCwd);fs::remove_all(temp);throw;}
}
