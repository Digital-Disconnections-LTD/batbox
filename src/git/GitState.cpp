#include "git/GitState.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::git {
namespace {
struct Collection {
  event_loop::Deferred<std::optional<GitRepoState>> output;
  GitRepoState value;
  std::size_t remaining=6;
  bool finished=false;
  explicit Collection(event_loop::Loop& loop):output(loop){}
  void complete(const std::exception_ptr& error){
    if(finished)return;
    if(error){finished=true;output.dispatcher().queueMicrotask([result=output]{
      result.resolve(std::nullopt);});return;}
    if(--remaining==0){finished=true;auto result=value;
      output.dispatcher().queueMicrotask([future=output,result=std::move(result)]{
        future.resolve(result);});}
  }
};
template<class T,class F>void add(const std::shared_ptr<Collection>& state,
  event_loop::Deferred<T> input,F&& set){
  input.then([state,set=std::forward<F>(set)](const T* value,
    const std::exception_ptr& error){
    if(error){state->complete(error);return;}
    try{set(state->value,*value);state->complete({});}
    catch(...){state->complete(std::current_exception());}
  });
}
}
event_loop::Deferred<std::optional<GitRepoState>> getGitState(
  event_loop::Loop& loop,const GitStateImports& imports){
  if(!loop.isOwnerThread())throw std::logic_error("Git state owner required");
  auto state=std::make_shared<Collection>(loop);
  try{
    if(!imports.getHead||!imports.getBranch||!imports.getRemoteUrl||
      !imports.getIsHeadOnRemote||!imports.getIsClean||!imports.getWorktreeCount)
      throw std::invalid_argument("Git state imports required");
    // Start all six operations in order before waiting for their combined result.
    auto head=imports.getHead();
    auto branch=imports.getBranch();
    auto remote=imports.getRemoteUrl();
    auto onRemote=imports.getIsHeadOnRemote();
    auto clean=imports.getIsClean();
    auto worktrees=imports.getWorktreeCount();
    add(state,head,[](GitRepoState& out,const std::string& value){out.commitHash=value;});
    add(state,branch,[](GitRepoState& out,const std::string& value){out.branchName=value;});
    add(state,remote,[](GitRepoState& out,const std::optional<std::string>& value){
      out.remoteUrl=value;});
    add(state,onRemote,[](GitRepoState& out,bool value){out.isHeadOnRemote=value;});
    add(state,clean,[](GitRepoState& out,bool value){out.isClean=value;});
    add(state,worktrees,[](GitRepoState& out,std::size_t value){out.worktreeCount=value;});
  }catch(...){state->output.resolve(std::nullopt);}
  return state->output;
}
} // namespace batbox::git
