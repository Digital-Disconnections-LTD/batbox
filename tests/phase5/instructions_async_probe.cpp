#include "instructions/project/async/AsyncDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>

using namespace batbox;
namespace project=batbox::instructions::project;
namespace async=batbox::instructions::project::async;
namespace fs=std::filesystem;
namespace {
Json projectFiles(const async::MemoryFiles& files){
  Json output=Json::array();
  for(const auto& file:files)output.push_back({{"path",file.path},
    {"type",file.type},{"content",file.content},
    {"parent",file.parent?Json(*file.parent):Json(nullptr)}});
  return output;
}
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> future){
  std::optional<T> result;std::exception_ptr error;
  future.then([&](const T* value,const std::exception_ptr& failure){
    if(failure)error=failure;else result=*value;});
  loop.runReady();if(error)std::rethrow_exception(error);
  if(!result)throw std::runtime_error("Instruction fixture promise pending");
  return *result;
}
bool inside(const std::string& child,const std::string& base){
  const auto rel=fs::path(child).lexically_relative(base).string();
  return rel.empty()||rel=="."||
    (rel!=".."&&!rel.starts_with("../")&&!fs::path(rel).is_absolute());
}
Json run(const std::string& scenario){
  event_loop::Loop loop;Json logs=Json::array();
  const std::map<std::string,std::string> files={
    {"/virtual/managed/BATBOX.md","Managed\n"},
    {"/virtual/project/BATBOX.md","Project\n@./child.md\n"},
    {"/virtual/project/child.md","Child\n"},
    {"/virtual/project/.batbox/rules/sub/nested.md","Nested\n"},
    {"/virtual/project/.batbox/rules/rule.md","Rule\n"}};
  int reads=0;bool throwCwd=scenario=="rejected-cache";
  bool hookAvailable=scenario!="late-hook";
  std::optional<event_loop::Deferred<std::string>> held;
  project::DiscoveryServices sync;
  sync.parse.paths.getCwd=[]{return "/virtual/project";};
  sync.parse.paths.homeDirectory=[]{return "/home/user";};
  sync.parse.paths.platform=[]{return "linux";};
  sync.parse.paths.normalizeNFC=[](const std::string& text){return text;};
  sync.parse.logDebug=[](const std::string&){};
  sync.excludePatterns=[]{return std::vector<std::string>{};};
  sync.pathInWorkingPath=inside;
  sync.originalCwd=[&]{logs.push_back("originalCwd");
    if(throwCwd)throw std::runtime_error("cwd failed");
    return "/virtual/project";};
  sync.externalIncludesApproved=[&]{logs.push_back("config");return false;};
  sync.memoryPath=[&](const std::string& type){logs.push_back("memoryPath:"+type);
    return std::string("/virtual/")+(type=="Managed"?"managed":"user")+"/BATBOX.md";};
  sync.managedRulesDir=[&]{logs.push_back("managedRules");
    return "/virtual/managed/.batbox/rules";};
  sync.userRulesDir=[&]{logs.push_back("userRules");
    return "/virtual/user/.batbox/rules";};
  sync.settingSourceEnabled=[&](const std::string& name){
    logs.push_back("setting:"+name);return name=="projectSettings";};
  sync.findGitRoot=[&](const std::string&)->std::optional<std::string>{
    logs.push_back("gitRoot");return std::nullopt;};
  sync.findCanonicalGitRoot=[&](const std::string&)->std::optional<std::string>{
    logs.push_back("canonicalRoot");return std::nullopt;};
  sync.additionalDirectoriesEnabled=[&]{logs.push_back("additionalEnabled");return false;};
  sync.autoMemoryEnabled=[&]{logs.push_back("autoEnabled");return false;};
  sync.teamMemoryEnabled=[]{return false;};
  sync.hasInstructionsLoadedHook=[&]{logs.push_back("hasHook");return hookAvailable;};
  sync.instructionsLoadedHook=[&](const project::MemoryFileInfo& file,
    const std::string& reason){logs.push_back("hook:"+file.path+":"+file.type+":"+reason);};
  async::Services services;services.sync=sync;
  services.readFile=[&](const std::string& path){
    logs.push_back("read:"+path);++reads;
    async::Deferred<std::string> promise(loop);
    if(scenario=="clear-inflight"&&reads==1){held=promise;return promise;}
    if(scenario=="rejected-io"&&path=="/virtual/project/BATBOX.md"){
      promise.reject(std::make_exception_ptr(std::runtime_error("ENOENT")));
      return promise;
    }
    const auto found=files.find(path);
    if(found==files.end())promise.reject(std::make_exception_ptr(std::runtime_error("ENOENT")));
    else promise.resolve(found->second);
    return promise;
  };
  services.readdir=[&](const std::string& path){
    logs.push_back("readdir:"+path);
    async::Deferred<std::vector<async::Dirent>> promise(loop);
    if(path=="/virtual/project/.batbox/rules")
      promise.resolve({{"sub",true,false},{"rule.md",false,true}});
    else if(path=="/virtual/project/.batbox/rules/sub")
      promise.resolve({{"nested.md",false,true}});
    else promise.resolve({});
    return promise;
  };
  services.stat=[&](const std::string&){
    async::Deferred<async::FileKind> promise(loop);
    promise.reject(std::make_exception_ptr(std::runtime_error("not symlink")));
    return promise;
  };
  async::MemoryDiscoveryStash stash(loop);
  if(scenario=="clear-inflight"){
    const auto first=stash.getMemoryFiles(services);
    const bool same=first.samePromise(stash.getMemoryFiles(services));
    stash.clearMemoryFileStashes();
    const auto second=stash.getMemoryFiles(services);
    const bool distinct=!first.samePromise(second);
    auto secondFiles=projectFiles(await(loop,second));
    logs.push_back("release:/virtual/managed/BATBOX.md");
    held->resolve(files.at("/virtual/managed/BATBOX.md"));
    auto firstFiles=projectFiles(await(loop,first));
    return {{"scenario",scenario},{"same",same},{"distinct",distinct},
      {"firstFiles",firstFiles},{"secondFiles",secondFiles},{"logs",logs}};
  }
  if(scenario=="rejected-cache"){
    const auto first=stash.getMemoryFiles(services);
    const bool same=first.samePromise(stash.getMemoryFiles(services));
    std::string error;
    try{await(loop,first);}catch(const std::exception& failure){error=failure.what();}
    const bool stillSame=first.samePromise(stash.getMemoryFiles(services));
    stash.clearMemoryFileStashes();throwCwd=false;
    auto recovered=projectFiles(await(loop,stash.getMemoryFiles(services)));
    return {{"scenario",scenario},{"same",same},{"stillSame",stillSame},
      {"error",error},{"recovered",recovered},{"logs",logs}};
  }
  if(scenario=="late-hook"){
    auto initial=projectFiles(await(loop,stash.getMemoryFiles(services)));
    hookAvailable=true;stash.clearMemoryFileStashes();
    auto afterClear=projectFiles(await(loop,stash.getMemoryFiles(services)));
    return {{"scenario",scenario},{"initial",initial},{"afterClear",afterClear},
      {"logs",logs}};
  }
  if(scenario=="forced-first"){
    auto forced=projectFiles(await(loop,stash.getMemoryFiles(true,services)));
    auto normal=projectFiles(await(loop,stash.getMemoryFiles(services)));
    return {{"scenario",scenario},{"forced",forced},{"normal",normal},
      {"logs",logs}};
  }
  const auto first=stash.getMemoryFiles(services);
  const bool same=first.samePromise(stash.getMemoryFiles(services));
  auto initial=projectFiles(await(loop,first));
  const auto explicitFalsePromise=stash.getMemoryFiles(false,services);
  const bool explicitFalseDistinct=!explicitFalsePromise.samePromise(first);
  auto explicitFalse=projectFiles(await(loop,explicitFalsePromise));
  const auto forced=stash.getMemoryFiles(true,services);
  const bool forcedDistinct=!first.samePromise(forced);
  auto forcedFiles=projectFiles(await(loop,forced));
  stash.clearMemoryFileStashes();
  auto afterClear=projectFiles(await(loop,stash.getMemoryFiles(services)));
  stash.resetGetMemoryFilesStash("compact");
  auto compact=projectFiles(await(loop,stash.getMemoryFiles(services)));
  return {{"scenario",scenario},{"same",same},
    {"explicitFalseDistinct",explicitFalseDistinct},{"forcedDistinct",forcedDistinct},
    {"initial",initial},{"explicitFalse",explicitFalse},
    {"forcedFiles",forcedFiles},{"afterClear",afterClear},
    {"compact",compact},{"logs",logs}};
}
}
int main(){
  Json rows=Json::array();
  for(const auto& scenario:{"normal","rejected-io","clear-inflight","rejected-cache",
    "late-hook","forced-first"})
    rows.push_back(run(scenario));
  std::cout<<value::stringify(rows)<<'\n';
}
