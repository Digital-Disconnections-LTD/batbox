#include "git/GitBasics.hpp"
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
Deferred<bool> GitBasics::dirIsInGitRepo(const std::string& cwd){
  owner();Deferred<bool> result(loop_);
  try{need(imports_.findGitRoot,"findGitRoot");
    result.resolve(imports_.findGitRoot(cwd).has_value());}
  catch(...){result.reject(std::current_exception());}
  return result;
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
std::string GitBasics::gitExe(){
  owner();if(gitExeStash_)return *gitExeStash_;
  need(imports_.whichSync,"whichSync");
  auto located=imports_.whichSync("git");
  gitExeStash_=located&& !located->empty()?*located:"git";
  return *gitExeStash_;
}
} // namespace batbox::git
