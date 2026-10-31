#include <cstdlib>
#include "instructions/project/async/AsyncDiscovery.hpp"
#include "instructions/project/async/ContextBinding.hpp"
#include "instructions/project/ProjectMemory.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <unistd.h>

using namespace batbox;
namespace project=batbox::instructions::project;
namespace async=batbox::instructions::project::async;
namespace fs=std::filesystem;
namespace {
void write(const fs::path& path,const std::string& content){
  fs::create_directories(path.parent_path());std::ofstream(path)<<content;
}
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;
  pending.then([&](const T* result,const std::exception_ptr& error){
    assert(loop.isOwnerThread());if(error)std::rethrow_exception(error);value=*result;});
  loop.run();assert(value);return *value;
}
bool inside(const std::string& path,const std::string& base){
  const auto relative=fs::path(path).lexically_relative(base).string();
  return relative.empty()||relative=="."||
    (relative!=".."&&!relative.starts_with("../")&&!fs::path(relative).is_absolute());
}
}
int main(){
  char raw[]="/tmp/batbox-async-instructions-XXXXXX";assert(::mkdtemp(raw));
  const fs::path temp=fs::canonical(raw),projectDir=temp/"project";
  try{
    write(temp/"managed/BATBOX.md","# Managed\n");
    write(temp/"user/BATBOX.md","# User\n");
    write(projectDir/"BATBOX.md","# Project\n@./child.txt\n");
    write(projectDir/"child.txt","# Child\n");
    write(projectDir/".batbox/rules/build.md","# Build\n");
    write(projectDir/".batbox/rules/sub/nested.md","# Nested\n");
    write(projectDir/"symlink-target.md","# Symlink\n");
    fs::create_symlink(projectDir/"symlink-target.md",
      projectDir/".batbox/rules/link.md");
    write(projectDir/"BATBOX.local.md","# Local\n");
    event_loop::Loop loop;project::DiscoveryServices sync;
    sync.parse.paths.getCwd=[&]{return projectDir.string();};
    sync.parse.paths.homeDirectory=[&]{return temp.string();};
    sync.parse.paths.platform=[]{return "linux";};
    sync.parse.paths.normalizeNFC=[](const std::string& value){return value;};
    sync.parse.logDebug=[](const std::string&){};
    sync.excludePatterns=[]{return std::vector<std::string>{};};
    sync.pathInWorkingPath=inside;
    sync.originalCwd=[&]{return projectDir.string();};
    sync.memoryPath=[&](const std::string& type){return (temp/
      (type=="Managed"?"managed":"user")/"BATBOX.md").string();};
    sync.managedRulesDir=[&]{return (temp/"managed/.batbox/rules").string();};
    sync.userRulesDir=[&]{return (temp/"user/.batbox/rules").string();};
    sync.settingSourceEnabled=[](const std::string&){return true;};
    sync.externalIncludesApproved=[]{return false;};
    sync.additionalDirectoriesEnabled=[]{return false;};
    sync.findGitRoot=[](const std::string&)->std::optional<std::string>{return std::nullopt;};
    sync.findCanonicalGitRoot=sync.findGitRoot;
    sync.autoMemoryEnabled=[]{return false;};
    sync.teamMemoryEnabled=[]{return false;};
    std::vector<std::string> hooks;
    sync.hasInstructionsLoadedHook=[]{return true;};
    sync.instructionsLoadedHook=[&](const project::MemoryFileInfo& file,
      const std::string& reason){hooks.push_back(file.path+":"+reason);};
    auto services=async::indigenousAsyncFs(loop,sync);
    async::MemoryDiscoveryStash stash(loop);
    context::AsyncServices contextServices;
    async::bindMemoryContextServices(contextServices,stash,services);
    const auto first=stash.getMemoryFiles(services);
    assert(first.samePromise(stash.getMemoryFiles(services)));
    assert(first.samePromise(contextServices.getMemoryFiles()));
    auto files=await(loop,first);
    assert(files.size()==8);
    assert(files[0].type=="Managed"&&files[1].type=="User");
    assert(files[2].path==(projectDir/"BATBOX.md").string());
    assert(files[3].path==(projectDir/"child.txt").string());
    assert(files[3].parent==(projectDir/"BATBOX.md").string());
    assert(files.back().type=="Local");
    bool foundBuild=false,foundNested=false,foundSymlink=false;
    for(const auto& file:files)
      {if(file.path==(projectDir/".batbox/rules/build.md").string())foundBuild=true;
       if(file.path==(projectDir/".batbox/rules/sub/nested.md").string())foundNested=true;
       if(file.path==(projectDir/"symlink-target.md").string())foundSymlink=true;}
    assert(foundBuild&&foundNested&&foundSymlink);
    assert(hooks.size()==8&&hooks[3].ends_with(":include"));
    const auto explicitFalse=stash.getMemoryFiles(false,services);
    assert(!explicitFalse.samePromise(first));
    assert(await(loop,explicitFalse).size()==8);
    assert(hooks.size()==8);
    const auto forced=stash.getMemoryFiles(true,services);
    assert(!forced.samePromise(first));
    assert(await(loop,forced).size()==8);
    assert(hooks.size()==8);
    stash.clearMemoryFileStashes();
    assert(await(loop,stash.getMemoryFiles(services)).size()==8);
    assert(hooks.size()==8);
    stash.resetGetMemoryFilesStash("compact");
    assert(await(loop,stash.getMemoryFiles(services)).size()==8);
    assert(hooks.size()==16&&hooks[8].ends_with(":compact"));
    fs::remove_all(temp);
    std::cout<<"owner-loop async instruction files/rules/cache/hook native fixture passed\n";
  }catch(...){fs::remove_all(temp);throw;}
}
