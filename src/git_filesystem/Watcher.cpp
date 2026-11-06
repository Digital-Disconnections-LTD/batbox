#include "git_filesystem/GitFilesystemInternal.hpp"
#include <stdexcept>

namespace batbox::git_filesystem {
void GitFilesystem::invalidate(){
  for(auto& [key,entry]:watcher_->stash){(void)key;entry.dirty=true;}
}
Deferred<Unit> GitFilesystem::ensureStarted(){
  if(watcher_->initialized)return resolved(loop_,Unit{});
  if(watcher_->initPromise)return *watcher_->initPromise;
  Deferred<Unit> output(loop_);watcher_->initPromise=output;
  resolveGitDir().then([this,output](const std::optional<std::string>* gitDir,
    const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    watcher_->initialized=true;
    if(!*gitDir){output.resolve(Unit{});return;}
    try{
      const auto headPath=joinPath(**gitDir,"HEAD");
      watcher_->watchedPaths.push_back(headPath);
      services_.watchFile(headPath,services_.watchIntervalMs,[this,alive=alive_]{
        if(alive->load())invalidate();});
      services_.registerCleanup([this,alive=alive_]{
        if(alive->load())stopWatching();});
      output.resolve(Unit{});
    }catch(...){output.reject(std::current_exception());}
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
  watcher_->watchedPaths.clear();
}
} // namespace batbox::git_filesystem
