#include "git_filesystem/GitFilesystemInternal.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::git_filesystem {
void GitFilesystem::invalidate(){
  for(auto& [key,entry]:watcher_->stash){(void)key;entry.dirty=true;}
}
Deferred<Unit> GitFilesystem::watchCurrentBranchRef(){
  Deferred<Unit> output(loop_);
  if(!watcher_->gitDir){output.resolve(Unit{});return output;}
  const auto gitDir=*watcher_->gitDir;
  readGitHead(gitDir).then([this,output,gitDir](const std::optional<Head>* head,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    const auto refsDir=watcher_->commonDir.value_or(gitDir);
    std::optional<std::string> refPath;
    if(*head&&head->value().type=="branch")
      refPath=joinPath(refsDir,"refs/heads/"+head->value().value);
    if(refPath==watcher_->branchRefPath){output.resolve(Unit{});return;}
    try{
      if(watcher_->branchRefPath){
        services_.unwatchFile(*watcher_->branchRefPath);
        std::erase(watcher_->watchedPaths,*watcher_->branchRefPath);
      }
      watcher_->branchRefPath=refPath;
      if(refPath){
        watcher_->watchedPaths.push_back(*refPath);
        services_.watchFile(*refPath,services_.watchIntervalMs,[this,alive=alive_]{
          if(alive->load())invalidate();});
      }
      output.resolve(Unit{});
    }catch(...){output.reject(std::current_exception());}
  });
  return output;
}
void GitFilesystem::onHeadChanged(){
  invalidate();
  try{services_.waitForScrollIdle().then([this](const Unit*,const std::exception_ptr& error){
    if(error)return;
    (void)watchCurrentBranchRef();
  });}catch(...){/* Watcher callbacks do not propagate asynchronous failures. */}
}
Deferred<Unit> GitFilesystem::ensureStarted(){
  if(watcher_->initialized)return resolved(loop_,Unit{});
  if(watcher_->initPromise)return *watcher_->initPromise;
  Deferred<Unit> output(loop_);watcher_->initPromise=output;
  resolveGitDir().then([this,output](const std::optional<std::string>* gitDir,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    watcher_->gitDir=*gitDir;
    watcher_->initialized=true;
    if(!*gitDir){output.resolve(Unit{});return;}
    getCommonDir(**gitDir).then([this,output,dir=**gitDir](
      const std::optional<std::string>* common,const std::exception_ptr& commonError){
      if(commonError){output.reject(commonError);return;}
      watcher_->commonDir=*common;
      try{
        const auto headPath=joinPath(dir,"HEAD");
        watcher_->watchedPaths.push_back(headPath);
        services_.watchFile(headPath,services_.watchIntervalMs,[this,alive=alive_]{
          if(alive->load())onHeadChanged();});
        const auto configPath=joinPath(common->value_or(dir),"config");
        watcher_->watchedPaths.push_back(configPath);
        services_.watchFile(configPath,services_.watchIntervalMs,[this,alive=alive_]{
          if(alive->load())invalidate();});
        watchCurrentBranchRef().then([this,output](const Unit*,
          const std::exception_ptr& watchError){
          if(watchError){output.reject(watchError);return;}
          try{services_.registerCleanup([this,alive=alive_]{
              if(alive->load())stopWatching();});
            output.resolve(Unit{});}
          catch(...){output.reject(std::current_exception());}
        });
      }catch(...){output.reject(std::current_exception());}
    });
  });
  return output;
}
Deferred<Json> GitFilesystem::getStashedValue(const std::string& key,
  std::function<Deferred<Json>()> compute){
  Deferred<Json> output(loop_);
  ensureStarted().then([this,output,key,compute](const Unit*,const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    auto existing=watcher_->stash.find(key);
    if(existing!=watcher_->stash.end()&&!existing->second.dirty){
      output.resolve(existing->second.value);return;
    }
    if(existing!=watcher_->stash.end())existing->second.dirty=false;
    try{compute().then([this,output,key,compute](const Json* value,
      const std::exception_ptr& computeError){
      if(computeError){output.reject(computeError);return;}
      auto entry=watcher_->stash.find(key);
      if(entry!=watcher_->stash.end()){
        if(!entry->second.dirty)entry->second.value=*value;
      }else watcher_->stash.emplace(key,Watcher::StashEntry{*value,false,compute});
      output.resolve(*value);
    });}catch(...){output.reject(std::current_exception());}
  });
  return output;
}
Deferred<std::string> GitFilesystem::getStashedBranch(){
  Deferred<std::string> output(loop_);
  getStashedValue("branch",[this]{
    Deferred<Json> adapted(loop_);
    computeBranch().then([adapted](const std::string* value,const std::exception_ptr& error){
      if(error)adapted.reject(error);else adapted.resolve(*value);
    });return adapted;
  }).then([output](const Json* value,const std::exception_ptr& error){
    if(error)output.reject(error);else output.resolve(value->get<std::string>());
  });return output;
}
Deferred<std::string> GitFilesystem::getStashedHead(){
  Deferred<std::string> output(loop_);
  getStashedValue("head",[this]{
    Deferred<Json> adapted(loop_);
    computeHead().then([adapted](const std::string* value,const std::exception_ptr& error){
      if(error)adapted.reject(error);else adapted.resolve(*value);
    });return adapted;
  }).then([output](const Json* value,const std::exception_ptr& error){
    if(error)output.reject(error);else output.resolve(value->get<std::string>());
  });return output;
}
Deferred<std::optional<std::string>> GitFilesystem::getStashedRemoteUrl(){
  Deferred<std::optional<std::string>> output(loop_);
  getStashedValue("remoteUrl",[this]{
    Deferred<Json> adapted(loop_);
    computeRemoteUrl().then([adapted](const std::optional<std::string>* value,
      const std::exception_ptr& error){
      if(error)adapted.reject(error);else adapted.resolve(*value?Json(**value):Json());
    });return adapted;
  }).then([output](const Json* value,const std::exception_ptr& error){
    if(error)output.reject(error);
    else output.resolve(value->is_null()?std::nullopt:std::optional(value->get<std::string>()));
  });return output;
}
Deferred<std::string> GitFilesystem::getStashedDefaultBranch(){
  Deferred<std::string> output(loop_);
  getStashedValue("defaultBranch",[this]{
    Deferred<Json> adapted(loop_);
    computeDefaultBranch().then([adapted](const std::string* value,
      const std::exception_ptr& error){
      if(error)adapted.reject(error);else adapted.resolve(*value);
    });return adapted;
  }).then([output](const Json* value,const std::exception_ptr& error){
    if(error)output.reject(error);else output.resolve(value->get<std::string>());
  });return output;
}
void GitFilesystem::stopWatching(){
  for(const auto& path:watcher_->watchedPaths)services_.unwatchFile(path);
  watcher_->watchedPaths.clear();watcher_->branchRefPath.reset();
}
void GitFilesystem::resetGitFileWatcher(){
  stopWatching();
  watcher_->stash.clear();watcher_->initialized=false;watcher_->initPromise.reset();
  watcher_->gitDir.reset();watcher_->commonDir.reset();
}
} // namespace batbox::git_filesystem
