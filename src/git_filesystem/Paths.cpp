#include "git_filesystem/GitFilesystemInternal.hpp"
#include "shell/PosixPath.hpp"
#include "runtime/IndigenousPath.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::git_filesystem {
std::string joinPath(const std::string& first,const std::string& second){
  return shell::joinPosixPath(first,second);
}
std::string resolvePath(const std::string& path){
  auto result=runtime::pathToUtf8(std::filesystem::absolute(
    runtime::pathFromUtf8(path.empty()?".":path)).lexically_normal());
  while(result.size()>1&&result.back()=='/')result.pop_back();
  return result;
}
std::string resolvePath(const std::string& first,const std::string& second){
  const auto next=runtime::pathFromUtf8(second);
  return resolvePath(next.is_absolute()?second:runtime::pathToUtf8(
    runtime::pathFromUtf8(first)/next));
}
GitFilesystem::GitFilesystem(event_loop::Loop& loop,Services services):
  loop_(loop),services_(std::move(services)),watcher_(std::make_shared<Watcher>()),
  alive_(std::make_shared<std::atomic_bool>(true)){
  if(!loop_.isOwnerThread())throw std::logic_error("GitFilesystem owner required");
  if(!services_.readFile||!services_.stat||!services_.readdir||!services_.findGitRoot||
     !services_.getCwd||!services_.parseGitConfigValue||!services_.waitForScrollIdle||
     !services_.watchFile||!services_.unwatchFile||!services_.registerCleanup)
    throw std::invalid_argument("Git filesystem imports required");
}
GitFilesystem::~GitFilesystem(){
  alive_->store(false);
  if(loop_.isOwnerThread())stopWatching();
}
void GitFilesystem::clearResolveGitDirStash(){gitDirStash_.clear();}
bool isSafeRefName(const std::string& name){
  if(name.empty()||name.front()=='-'||name.front()=='/'||name.find("..")!=std::string::npos)
    return false;
  std::size_t start=0;
  for(;;){
    const auto end=name.find('/',start);
    const auto part=name.substr(start,end==std::string::npos?std::string::npos:end-start);
    if(part.empty()||part==".")return false;
    if(end==std::string::npos)break;
    start=end+1;
  }
  for(unsigned char ch:name)
    if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
      (ch>='0'&&ch<='9')||ch=='/'||ch=='.'||ch=='_'||ch=='+'||ch=='-'||ch=='@'))
      return false;
  return true;
}
bool isValidGitSha(const std::string& sha){
  if(sha.size()!=40&&sha.size()!=64)return false;
  for(char ch:sha)if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')))return false;
  return true;
}
Deferred<std::optional<std::string>> GitFilesystem::resolveGitDir(
  std::optional<std::string> startPath){
  Deferred<std::optional<std::string>> output(loop_);
  try{
    const auto cwd=resolvePath(startPath?*startPath:services_.getCwd());
    const auto found=gitDirStash_.find(cwd);
    if(found!=gitDirStash_.end()){output.resolve(found->second);return output;}
    const auto root=services_.findGitRoot(cwd);
    if(!root){gitDirStash_[cwd]=std::nullopt;output.resolve(std::nullopt);return output;}
    const auto gitPath=joinPath(*root,".git");
    services_.stat(gitPath).then([this,output,cwd,root,gitPath](
      const FileStat* stat,const std::exception_ptr& error){
      if(error){gitDirStash_[cwd]=std::nullopt;output.resolve(std::nullopt);return;}
      if(!stat->isFile){gitDirStash_[cwd]=gitPath;output.resolve(gitPath);return;}
      try{services_.readFile(gitPath).then([this,output,cwd,root,gitPath](
        const std::string* content,const std::exception_ptr& readError){
        if(readError){gitDirStash_[cwd]=std::nullopt;output.resolve(std::nullopt);return;}
        try{
          const auto text=value::trim(*content);
          if(text.rfind("gitdir:",0)==0){
            const auto path=resolvePath(*root,value::trim(text.substr(7)));
            gitDirStash_[cwd]=path;output.resolve(path);return;
          }
          gitDirStash_[cwd]=gitPath;output.resolve(gitPath);
        }catch(...){gitDirStash_[cwd]=std::nullopt;output.resolve(std::nullopt);}
      });}catch(...){gitDirStash_[cwd]=std::nullopt;output.resolve(std::nullopt);}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::git_filesystem
