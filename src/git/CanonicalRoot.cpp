#include "git/CanonicalRoot.hpp"
#include "git/filesystem/GitFilesystemInternal.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Unicode.hpp"
#include <filesystem>
#include <stdexcept>
#include <system_error>
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
  if(!imports_.statSync||!imports_.readFileSync||!imports_.realpathSync)
    throw std::invalid_argument("Canonical Git root imports required");
}
CanonicalRoot::Entry& CanonicalRoot::touch(const std::string& key){
  const auto found=stash_.find(key);
  if(found!=stash_.end()){
    recency_.splice(recency_.begin(),recency_,found->second.recency);
    return found->second;
  }
  recency_.push_front(key);
  const auto inserted=stash_.emplace(key,Entry{{},std::nullopt,recency_.begin()});
  if(stash_.size()>50){stash_.erase(recency_.back());recency_.pop_back();}
  return inserted.first->second;
}
std::optional<std::string> CanonicalRoot::walk(const std::string& startPath)const{
  auto current=git::filesystem::resolvePath(startPath);
  constexpr char separator=static_cast<char>(std::filesystem::path::preferred_separator);
  const auto slash=current.find(separator);
  const auto root=slash==std::string::npos?std::string(1,separator):current.substr(0,slash+1);
  while(current!=root){
    try{
      const auto st=imports_.statSync(git::filesystem::joinPath(current,".git"));
      if(st.isDirectory||st.isFile)return value::normalizeNFC(current);
    }catch(...){}
    const auto parent=runtime::pathToUtf8(runtime::pathFromUtf8(current).parent_path());
    if(parent==current)break;
    current=parent;
  }
  try{
    const auto st=imports_.statSync(git::filesystem::joinPath(root,".git"));
    if(st.isDirectory||st.isFile)return value::normalizeNFC(root);
  }catch(...){}
  return std::nullopt;
}
std::optional<std::string> CanonicalRoot::find(const std::string& startPath){
  auto&slot=touch(startPath);
  if(!slot.root)slot.root=walk(startPath);
  if(!slot.root||slot.root->empty())return std::nullopt;
  if(!slot.value)slot.value=canonical(*slot.root);
  return slot.value;
}
std::optional<std::string> CanonicalRoot::nearest(const std::string& startPath){
  auto&slot=touch(startPath);
  if(!slot.root)slot.root=walk(startPath);
  return slot.root;
}
std::string CanonicalRoot::resolve(const std::string& gitRoot){
  auto&slot=touch(gitRoot);
  if(!slot.value)slot.value=canonical(gitRoot);
  return *slot.value;
}
std::string CanonicalRoot::canonical(const std::string& gitRoot)const{
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
} // namespace batbox::git
