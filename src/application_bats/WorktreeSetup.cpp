#include "application_bats/Worktree.hpp"
#include "glob/Ignore.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace batbox::application_bats {
namespace {
namespace fs=std::filesystem;
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string trim(const std::string& value){return value::trim(value);}
std::vector<std::string> lines(const std::string& content){
  std::vector<std::string> result;
  std::istringstream stream(content);
  std::string line;
  while(std::getline(stream,line)){
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    result.push_back(std::move(line));
  }
  return result;
}
std::vector<std::string> paths(const Json& value,const char* key){
  if(value.is_null())return {};
  if(!value.is_array())throw std::invalid_argument(
    std::string("worktree.")+key+" must be an array of paths");
  std::vector<std::string> result;
  for(const auto& item:value){
    if(!item.is_string())throw std::invalid_argument(
      std::string("worktree.")+key+" must be an array of paths");
    result.push_back(item.get<std::string>());
  }
  return result;
}
bool hasTraversal(const std::string& path){
  std::string part;
  for(const char c:path){
    if(c=='/'||c=='\\'){
      if(part=="..")return true;
      part.clear();
    }else part+=c;
  }
  return part=="..";
}
bool startsWith(const std::string& text,const std::string& prefix){
  return text.rfind(prefix,0)==0;
}
} // namespace

WorktreeSettings parseWorktreeSettings(const Json& settings){
  WorktreeSettings result;
  const auto& worktree=field(settings,"worktree");
  if(!worktree.is_object())return result;
  result.sparsePaths=paths(field(worktree,"sparsePaths"),"sparsePaths");
  result.symlinkDirectories=paths(field(worktree,"symlinkDirectories"),
    "symlinkDirectories");
  return result;
}

void WorktreeProvider::copyWorktreeIncludeFiles(const std::string& root,
    const std::string& path)const{
  const fs::path rootPath=runtime::pathFromUtf8(root);
  const fs::path worktreePath=runtime::pathFromUtf8(path);
  std::ifstream input(rootPath/".worktreeinclude",std::ios::binary);
  if(!input)return;
  const std::string content((std::istreambuf_iterator<char>(input)),
    std::istreambuf_iterator<char>());
  // Trimmed lines select collapsed directories for
  // expansion. node-ignore receives the original .worktreeinclude text.
  const auto matcherPatterns=lines(content);
  std::vector<std::string> patterns;
  for(const auto& line:matcherPatterns){
    const auto pattern=trim(line);
    if(!pattern.empty()&&pattern.front()!='#')patterns.push_back(pattern);
  }
  if(patterns.empty())return;
  const auto ignored=git(root,{"ls-files","--others","--ignored",
    "--exclude-standard","--directory"});
  if(ignored.code!=0||trim(ignored.stdoutText).empty())return;
  std::vector<std::string> collapsed;
  std::vector<std::string> files;
  for(const auto& entry:lines(trim(ignored.stdoutText))){
    if(entry.empty())continue;
    if(entry.back()=='/')collapsed.push_back(entry);
    else if(glob::ignores(entry,matcherPatterns))files.push_back(entry);
  }
  std::vector<std::string> expand;
  for(const auto& dir:collapsed){
    bool selected=glob::ignores(dir.substr(0,dir.size()-1),matcherPatterns);
    for(const auto& pattern:patterns){
      const auto normalized=pattern.front()=='/'?pattern.substr(1):pattern;
      if(startsWith(normalized,dir)){selected=true;break;}
      const auto globIndex=normalized.find_first_of("*?[");
      if(globIndex!=std::string::npos&&globIndex>0&&
          startsWith(dir,normalized.substr(0,globIndex))){
        selected=true;break;
      }
    }
    if(selected)expand.push_back(dir);
  }
  if(!expand.empty()){
    std::vector<std::string> args={"ls-files","--others","--ignored",
      "--exclude-standard","--"};
    args.insert(args.end(),expand.begin(),expand.end());
    const auto expanded=git(root,args);
    if(expanded.code==0)for(const auto& entry:lines(trim(expanded.stdoutText)))
      if(!entry.empty()&&glob::ignores(entry,matcherPatterns))files.push_back(entry);
  }
  for(const auto& relative:files){
    try{
      // Git prints repository-relative paths; reject any malformed path that
      // could escape either tree before the best-effort copy.
      const fs::path rel=runtime::pathFromUtf8(relative);
      if(rel.is_absolute()||hasTraversal(relative))continue;
      const auto target=worktreePath/rel;
      fs::create_directories(target.parent_path());
      fs::copy_file(rootPath/rel,target,fs::copy_options::overwrite_existing);
    }catch(const std::exception& failure){
      if(inputs_.diagnostic)inputs_.diagnostic(
        "Failed to copy "+relative+" to worktree: "+failure.what());
    }
  }
}

void WorktreeProvider::postCreationSetup(const std::string& root,
    const std::string& path,const WorktreeSettings& settings)const{
  const fs::path rootPath=runtime::pathFromUtf8(root);
  const fs::path worktreePath=runtime::pathFromUtf8(path);
  std::error_code error;
  const auto localSettings=rootPath/".batbox"/"settings.local.json";
  if(fs::is_regular_file(localSettings,error)){
    const auto destination=worktreePath/".batbox"/"settings.local.json";
    fs::create_directories(destination.parent_path(),error);
    if(!error)fs::copy_file(localSettings,destination,
      fs::copy_options::overwrite_existing,error);
    if(error&&inputs_.diagnostic)
      inputs_.diagnostic("Failed to copy settings.local.json: "+error.message());
  }
  auto hooksPath=rootPath/".husky";
  if(!fs::is_directory(hooksPath,error))hooksPath=rootPath/".git"/"hooks";
  if(fs::is_directory(hooksPath,error)){
    const auto absolute=runtime::pathToUtf8(hooksPath);
    const auto prior=git(path,{"config","--get","core.hooksPath"});
    if(prior.code!=0||trim(prior.stdoutText)!=absolute){
      const auto configured=git(path,{"config","core.hooksPath",absolute});
      if(configured.code!=0&&inputs_.diagnostic)
        inputs_.diagnostic("Failed to configure worktree hooks: "+
          trim(configured.stderrText));
    }
  }
  for(const auto& relative:settings.symlinkDirectories){
    if(hasTraversal(relative)||runtime::pathFromUtf8(relative).is_absolute()){
      if(inputs_.diagnostic)inputs_.diagnostic(
        "Skipping symlink for \""+relative+"\": path traversal detected");
      continue;
    }
    const auto source=rootPath/runtime::pathFromUtf8(relative);
    const auto target=worktreePath/runtime::pathFromUtf8(relative);
    fs::create_directory_symlink(source,target,error);
    if(error&&error!=std::errc::no_such_file_or_directory&&
        error!=std::errc::file_exists&&inputs_.diagnostic)
      inputs_.diagnostic("Failed to symlink "+relative+": "+error.message());
  }
  copyWorktreeIncludeFiles(root,path);
}
} // namespace batbox::application_bats
