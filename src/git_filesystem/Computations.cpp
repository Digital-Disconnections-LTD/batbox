#include "git_filesystem/GitFilesystemInternal.hpp"
#include <stdexcept>

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
Deferred<std::string> GitFilesystem::computeHead(){
  Deferred<std::string> output(loop_);
  resolveGitDir().then([this,output](const std::optional<std::string>* gitDir,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    if(!*gitDir){output.resolve("");return;}
    readGitHead(**gitDir).then([this,output,dir=**gitDir](
      const std::optional<Head>* head,const std::exception_ptr& headError){
      if(headError){output.reject(headError);return;}
      if(!*head){output.resolve("");return;}
      if(head->value().type=="branch"){
        resolveRef(dir,"refs/heads/"+head->value().value).then(
          [output](const std::optional<std::string>* sha,const std::exception_ptr& refError){
          if(refError)output.reject(refError);else output.resolve(sha->value_or(""));
        });
      }else output.resolve(head->value().value);
    });
  });
  return output;
}
Deferred<std::optional<std::string>> GitFilesystem::computeRemoteUrl(){
  Deferred<std::optional<std::string>> output(loop_);
  resolveGitDir().then([this,output](const std::optional<std::string>* gitDir,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    if(!*gitDir){output.resolve(std::nullopt);return;}
    const auto dir=**gitDir;
    try{services_.parseGitConfigValue(dir,"remote",std::string("origin"),"url").then(
      [this,output,dir](const std::optional<std::string>* url,const std::exception_ptr& readError){
      if(readError){output.reject(readError);return;}
      if(*url&&!url->value().empty()){output.resolve(*url);return;}
      getCommonDir(dir).then([this,output,dir](const std::optional<std::string>* common,
        const std::exception_ptr& commonError){
        if(commonError){output.reject(commonError);return;}
        if(*common&&**common!=dir){
          try{event_loop::adoptAsyncReturn(services_.parseGitConfigValue(
            **common,"remote",std::string("origin"),"url")).then(
            [output](const std::optional<std::string>* value,const std::exception_ptr& error){
            if(error)output.reject(error);else output.resolve(*value);
          });}catch(...){output.reject(std::current_exception());}
        }else output.resolve(std::nullopt);
      });
    });}catch(...){output.reject(std::current_exception());}
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
Deferred<std::optional<std::string>> GitFilesystem::getHeadForDir(const std::string& cwd){
  Deferred<std::optional<std::string>> output(loop_);
  resolveGitDir(cwd).then([this,output](const std::optional<std::string>* gitDir,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    if(!*gitDir){output.resolve(std::nullopt);return;}
    readGitHead(**gitDir).then([this,output,dir=**gitDir](
      const std::optional<Head>* head,const std::exception_ptr& headError){
      if(headError){output.reject(headError);return;}
      if(!*head){output.resolve(std::nullopt);return;}
      if(head->value().type=="branch"){
        event_loop::adoptAsyncReturn(resolveRef(dir,"refs/heads/"+head->value().value)).then(
          [output](const std::optional<std::string>* sha,const std::exception_ptr& refError){
          if(refError)output.reject(refError);else output.resolve(*sha);
        });
      }else output.resolve(head->value().value);
    });
  });
  return output;
}
Deferred<std::optional<std::string>> GitFilesystem::readWorktreeHeadSha(
  const std::string& worktreePath){
  Deferred<std::optional<std::string>> output(loop_);
  try{services_.readFile(joinPath(worktreePath,".git")).then(
    [this,output,worktreePath](const std::string* pointer,
      const std::exception_ptr& error){
      if(error){output.resolve(std::nullopt);return;}
      const auto text=value::trim(*pointer);
      if(text.rfind("gitdir:",0)!=0){output.resolve(std::nullopt);return;}
      const auto gitDir=resolvePath(worktreePath,value::trim(text.substr(7)));
      readGitHead(gitDir).then([this,output,gitDir](
        const std::optional<Head>* head,const std::exception_ptr& headError){
        if(headError){output.reject(headError);return;}
        if(!*head){output.resolve(std::nullopt);return;}
        if(head->value().type=="branch"){
          event_loop::adoptAsyncReturn(resolveRef(gitDir,"refs/heads/"+head->value().value)).then(
            [output](const std::optional<std::string>* sha,const std::exception_ptr& refError){
            if(refError)output.reject(refError);else output.resolve(*sha);
          });
        }else output.resolve(head->value().value);
      });
    });}catch(...){output.resolve(std::nullopt);}
  return output;
}
Deferred<std::optional<std::string>> GitFilesystem::getRemoteUrlForDir(
  const std::string& cwd){
  Deferred<std::optional<std::string>> output(loop_);
  resolveGitDir(cwd).then([this,output](const std::optional<std::string>* gitDir,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    if(!*gitDir){output.resolve(std::nullopt);return;}
    const auto dir=**gitDir;
    try{services_.parseGitConfigValue(dir,"remote",std::string("origin"),"url").then(
      [this,output,dir](const std::optional<std::string>* url,const std::exception_ptr& parseError){
      if(parseError){output.reject(parseError);return;}
      if(*url&&!url->value().empty()){output.resolve(*url);return;}
      getCommonDir(dir).then([this,output,dir](const std::optional<std::string>* common,
        const std::exception_ptr& commonError){
        if(commonError){output.reject(commonError);return;}
        if(*common&&**common!=dir){
          try{event_loop::adoptAsyncReturn(services_.parseGitConfigValue(
            **common,"remote",std::string("origin"),"url")).then(
            [output](const std::optional<std::string>* value,const std::exception_ptr& readError){
            if(readError)output.reject(readError);else output.resolve(*value);
          });}catch(...){output.reject(std::current_exception());}
        }else output.resolve(std::nullopt);
      });
    });}catch(...){output.reject(std::current_exception());}
  });
  return output;
}
Deferred<bool> GitFilesystem::isShallowClone(){
  Deferred<bool> output(loop_);
  resolveGitDir().then([this,output](const std::optional<std::string>* gitDir,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    if(!*gitDir){output.resolve(false);return;}
    getCommonDir(**gitDir).then([this,output,dir=**gitDir](
      const std::optional<std::string>* common,const std::exception_ptr& commonError){
      if(commonError){output.reject(commonError);return;}
      try{services_.stat(joinPath(common->value_or(dir),"shallow")).then(
        [output](const FileStat*,const std::exception_ptr& statError){
        output.resolve(!statError);
      });}catch(...){output.resolve(false);}
    });
  });
  return output;
}
Deferred<std::size_t> GitFilesystem::getWorktreeCountFromFs(){
  Deferred<std::size_t> output(loop_);
  resolveGitDir().then([this,output](const std::optional<std::string>* gitDir,
    const std::exception_ptr& error){
    if(error){output.resolve(1);return;}
    if(!*gitDir){output.resolve(0);return;}
    getCommonDir(**gitDir).then([this,output,dir=**gitDir](
      const std::optional<std::string>* common,const std::exception_ptr& commonError){
      if(commonError){output.resolve(1);return;}
      try{services_.readdir(joinPath(common->value_or(dir),"worktrees")).then(
        [output](const std::vector<std::string>* entries,const std::exception_ptr& readError){
        if(readError)output.resolve(1);else output.resolve(entries->size()+1);
      });}catch(...){output.resolve(1);}
    });
  });
  return output;
}
} // namespace batbox::git_filesystem
