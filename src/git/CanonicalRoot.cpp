#include "git/CanonicalRoot.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Unicode.hpp"
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace batbox::git {
namespace {
namespace fs=std::filesystem;
std::string pathJoin(const std::string& first,const std::string& second){
  return runtime::pathToUtf8((runtime::pathFromUtf8(first)/
    runtime::pathFromUtf8(second)).lexically_normal());
}
std::string pathResolve(const std::string& first,const std::string& second){
  const auto tail=runtime::pathFromUtf8(second);
  const auto resolved=fs::absolute(tail.is_absolute()?tail:
    runtime::pathFromUtf8(first)/tail).lexically_normal();
  auto result=runtime::pathToUtf8(resolved);
  const auto rootLength=runtime::pathToUtf8(resolved.root_path()).size();
  // Node resolve removes a directory's trailing separator, including an
  // empty final component, while keeping the indigenous root intact.
  while(result.size()>rootLength&&result.back()==
      static_cast<char>(fs::path::preferred_separator))result.pop_back();
  return result;
}
std::string dirname(const std::string& path){return runtime::pathToUtf8(
  runtime::pathFromUtf8(path).parent_path());}
std::string basename(const std::string& path){return runtime::pathToUtf8(
  runtime::pathFromUtf8(path).filename());}
}
CanonicalRoot::CanonicalRoot(CanonicalRootImports imports):imports_(std::move(imports)){
  if(!imports_.findGitRoot||!imports_.readFileSync||!imports_.realpathSync)
    throw std::invalid_argument("Canonical Git root imports required");
}
std::optional<std::string> CanonicalRoot::find(const std::string& startPath){
  const auto root=imports_.findGitRoot(startPath);
  if(!root||root->empty())return std::nullopt;
  return resolve(*root);
}
std::string CanonicalRoot::resolve(const std::string& gitRoot){
  const auto found=stash_.find(gitRoot);
  if(found!=stash_.end()){
    recency_.splice(recency_.begin(),recency_,found->second.recency);
    return found->second.value;
  }
  auto value=compute(gitRoot);
  recency_.push_front(gitRoot);
  stash_.emplace(gitRoot,Entry{value,recency_.begin()});
  if(stash_.size()>50){stash_.erase(recency_.back());recency_.pop_back();}
  return value;
}
std::string CanonicalRoot::compute(const std::string& gitRoot)const{
  try{
    const auto gitContent=value::trim(imports_.readFileSync(pathJoin(gitRoot,".git")));
    if(gitContent.rfind("gitdir:",0)!=0)return gitRoot;
    const auto worktreeGitDir=pathResolve(gitRoot,value::trim(value::slice(gitContent,7)));
    const auto commonDir=pathResolve(worktreeGitDir,
      value::trim(imports_.readFileSync(pathJoin(worktreeGitDir,"commondir"))));
    // Both the worktree location and its backlink must match Git's layout.
    if(pathResolve(dirname(worktreeGitDir),"")!=pathJoin(commonDir,"worktrees"))
      return gitRoot;
    const auto backlink=imports_.realpathSync(value::trim(imports_.readFileSync(
      pathJoin(worktreeGitDir,"gitdir"))));
    if(backlink!=pathJoin(imports_.realpathSync(gitRoot),".git"))return gitRoot;
    if(basename(commonDir)!=".git")return value::normalizeNFC(commonDir);
    return value::normalizeNFC(dirname(commonDir));
  }catch(...){return gitRoot;}
}
void CanonicalRoot::clear(){stash_.clear();recency_.clear();}
std::size_t CanonicalRoot::stashSize()const{return stash_.size();}
bool CanonicalRoot::stashDelete(const std::string& root){
  const auto found=stash_.find(root);if(found==stash_.end())return false;
  recency_.erase(found->second.recency);stash_.erase(found);return true;
}
std::optional<std::string> CanonicalRoot::stashGet(const std::string& root)const{
  const auto found=stash_.find(root);
  return found==stash_.end()?std::nullopt:std::optional<std::string>(found->second.value);
}
bool CanonicalRoot::stashHas(const std::string& root)const{return stash_.contains(root);}
} // namespace batbox::git
