#include "git/GitBasics.hpp"
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::git {
namespace {
template<class T>void need(const std::function<T>& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Git import required: ")+name);
}
template<class T,class F>Deferred<T> asyncReturn(event_loop::Loop& loop,F&& body){
  try{return event_loop::adoptAsyncReturn(body());}
  catch(...){Deferred<T> result(loop);result.reject(std::current_exception());return result;}
}
}
GitBasics::GitBasics(event_loop::Loop& loop,BasicsImports imports):
  loop_(loop),imports_(std::move(imports)){}
void GitBasics::owner()const{
  if(!loop_.isOwnerThread())throw std::logic_error("Git state requires event loop owner");
}
Deferred<bool> GitBasics::getIsGit(){
  owner();if(isGitStash_)return *isGitStash_;
  Deferred<bool> result(loop_);
  try{
    need(imports_.nowMillis,"nowMillis");
    need(imports_.logDiagnostic,"logDiagnostic");
    const auto startTime=imports_.nowMillis();
    imports_.logDiagnostic("info","is_git_check_started",std::nullopt);
    need(imports_.getCwd,"getCwd");need(imports_.findGitRoot,"findGitRoot");
    const bool isGit=imports_.findGitRoot(imports_.getCwd()).has_value();
    const auto duration=imports_.nowMillis()-startTime;
    imports_.logDiagnostic("info","is_git_check_completed",
      Json{{"duration_ms",duration},{"is_git",isGit}});
    result.resolve(isGit);
  }catch(...){result.reject(std::current_exception());}
  isGitStash_=result;return result;
}
Deferred<std::optional<std::string>> GitBasics::getGitDir(const std::string& cwd){
  owner();need(imports_.resolveGitDir,"resolveGitDir");
  return imports_.resolveGitDir(cwd); // Return the Promise directly.
}
Deferred<bool> GitBasics::isAtGitRoot(){
  owner();Deferred<bool> result(loop_);
  try{
    need(imports_.getCwd,"getCwd");need(imports_.findGitRoot,"findGitRoot");
    const auto cwd=imports_.getCwd();
    const auto root=imports_.findGitRoot(cwd);
    if(!root||root->empty()){
      result.resolve(false);
      return result;
    }
    Deferred<std::string> resolvedCwd(loop_),resolvedRoot(loop_);
    try{
      need(imports_.realpath,"realpath");
      resolvedCwd=imports_.realpath(cwd);
      resolvedRoot=imports_.realpath(*root);
    }catch(...){result.resolve(cwd==*root);return result;}
    struct Wait {
      std::optional<std::string> cwd,root;
      bool completed=false;
    };
    auto state=std::make_shared<Wait>();
    auto continuation=[state,result,cwd,root]{
      if(state->completed)return;
      if(state->cwd&&state->root){state->completed=true;
        result.dispatcher().queueMicrotask([result,state]{
          result.resolve(*state->cwd==*state->root);});}
    };
    auto fallback=[state,result,cwd,root]{
      if(state->completed)return;
      state->completed=true;
      result.dispatcher().queueMicrotask([result,cwd,root]{result.resolve(cwd==*root);});
    };
    resolvedCwd.then([state,continuation,fallback](const std::string* value,
      const std::exception_ptr& error){
      if(error){fallback();return;}state->cwd=*value;continuation();
    });
    resolvedRoot.then([state,continuation,fallback](const std::string* value,
      const std::exception_ptr& error){
      if(error){fallback();return;}state->root=*value;continuation();
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
Deferred<bool> GitBasics::dirIsInGitRepo(const std::string& cwd){
  owner();Deferred<bool> result(loop_);
  try{need(imports_.findGitRoot,"findGitRoot");
    result.resolve(imports_.findGitRoot(cwd).has_value());}
  catch(...){result.reject(std::current_exception());}
  return result;
}
Deferred<std::string> GitBasics::getHead(){
  owner();return asyncReturn<std::string>(loop_,[this]{
    need(imports_.getStashedHead,"getCachedHead");return imports_.getStashedHead();});
}
Deferred<std::string> GitBasics::getBranch(){
  owner();return asyncReturn<std::string>(loop_,[this]{
    need(imports_.getStashedBranch,"getCachedBranch");return imports_.getStashedBranch();});
}
Deferred<std::string> GitBasics::getDefaultBranch(){
  owner();return asyncReturn<std::string>(loop_,[this]{
    need(imports_.getStashedDefaultBranch,"getCachedDefaultBranch");
    return imports_.getStashedDefaultBranch();});
}
Deferred<std::optional<std::string>> GitBasics::getRemoteUrl(){
  owner();return asyncReturn<std::optional<std::string>>(loop_,[this]{
    need(imports_.getStashedRemoteUrl,"getCachedRemoteUrl");
    return imports_.getStashedRemoteUrl();});
}
Deferred<std::size_t> GitBasics::getWorktreeCount(){
  owner();return asyncReturn<std::size_t>(loop_,[this]{
    need(imports_.getWorktreeCountFromFs,"getWorktreeCountFromFs");
    return imports_.getWorktreeCountFromFs();});
}
Deferred<bool> GitBasics::isShallowClone(){
  owner();need(imports_.isShallowCloneFs,"isShallowCloneFs");
  return imports_.isShallowCloneFs(); // Return the service Promise directly.
}
std::string GitBasics::gitExe(){
  owner();if(gitExeStash_)return *gitExeStash_;
  need(imports_.whichSync,"whichSync");
  auto located=imports_.whichSync("git");
  gitExeStash_=located&& !located->empty()?*located:"git";
  return *gitExeStash_;
}
} // namespace batbox::git
