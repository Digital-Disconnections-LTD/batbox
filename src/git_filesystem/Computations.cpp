#include "git_filesystem/GitFilesystemInternal.hpp"

namespace batbox::git_filesystem {
Deferred<std::string> GitFilesystem::computeBranch(){
  Deferred<std::string> output(loop_);
  resolveGitDir().then([this,output](const std::optional<std::string>* gitDir,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    if(!*gitDir){output.resolve("HEAD");return;}
    readGitHead(**gitDir).then([output](const std::optional<Head>* head,
      const std::exception_ptr& headError){
      if(headError)output.reject(headError);
      else output.resolve(*head&&head->value().type=="branch"?head->value().value:"HEAD");
    });
  });
  return output;
}
Deferred<std::string> GitFilesystem::computeDefaultBranch(){
  Deferred<std::string> output(loop_);
  resolveGitDir().then([this,output](const std::optional<std::string>* gitDir,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    if(!*gitDir){output.resolve("main");return;}
    getCommonDir(**gitDir).then([this,output,dir=**gitDir](
      const std::optional<std::string>* common,const std::exception_ptr& commonError){
      if(commonError){output.reject(commonError);return;}
      const auto refsDir=common->value_or(dir);
      readRawSymref(refsDir,"refs/remotes/origin/HEAD","refs/remotes/origin/").then(
        [this,output,refsDir](const std::optional<std::string>* branch,
          const std::exception_ptr& branchError){
        if(branchError){output.reject(branchError);return;}
        if(*branch&&!branch->value().empty()){output.resolve(**branch);return;}
        resolveRef(refsDir,"refs/remotes/origin/main").then(
          [this,output,refsDir](const std::optional<std::string>* main,
            const std::exception_ptr& mainError){
          if(mainError){output.reject(mainError);return;}
          if(*main&&!main->value().empty()){output.resolve("main");return;}
          resolveRef(refsDir,"refs/remotes/origin/master").then(
            [output](const std::optional<std::string>* master,
              const std::exception_ptr& masterError){
            if(masterError)output.reject(masterError);
            else output.resolve(*master&&!master->value().empty()?"master":"main");
          });
        });
      });
    });
  });
  return output;
}
} // namespace batbox::git_filesystem
