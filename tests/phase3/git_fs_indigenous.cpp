#include <cstdlib>
#include "git_filesystem/GitFilesystem.hpp"
#include "git_filesystem/RootFinder.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <unistd.h>

using namespace batbox::git_filesystem;
namespace fs=std::filesystem;
namespace {
void write(const fs::path& path,const std::string& text){
  fs::create_directories(path.parent_path());std::ofstream(path)<<text;
}
template<class T>T await(batbox::event_loop::Loop& loop,Deferred<T> pending){
  std::optional<T> result;
  pending.then([&](const T* value,const std::exception_ptr& error){
    assert(loop.isOwnerThread());if(error)std::rethrow_exception(error);result=*value;});
  loop.run();assert(result);return *result;
}
void pauseLoop(batbox::event_loop::Loop& loop,double milliseconds){
  loop.setTimeout(milliseconds,[]{});loop.run();
}
}
int main(){
  char temp[]="/tmp/batbox-git-fs-XXXXXX";assert(::mkdtemp(temp));
  const fs::path root=temp;
  try{
    write(root/".git/HEAD","ref: refs/heads/main\n");
    write(root/".git/refs/heads/main",std::string(40,'a')+"\n");
    write(root/".git/refs/heads/feature",std::string(40,'b')+"\n");
    batbox::event_loop::Loop loop;
    auto services=indigenousGitFsServices(loop);
    services.getCwd=[root]{return root.string();};
    auto finder=indigenousRootFinder();
    services.findGitRoot=[finder](const std::string& cwd){return finder->find(cwd);};
    services.registerCleanup=[](std::function<void()>){};
    services.watchIntervalMs=20;
    GitFilesystem git(loop,services);
    assert(await(loop,git.getStashedBranch())=="main");
    assert(await(loop,git.getStashedDefaultBranch())=="main");
    pauseLoop(loop,50);
    write(root/".git/HEAD","ref: refs/heads/feature\n");
    pauseLoop(loop,80);
    assert(await(loop,git.getStashedBranch())=="feature");
    fs::remove_all(root);
    std::cout<<"native Git filesystem and ref watcher passed\n";
  }catch(...){fs::remove_all(root);throw;}
}
