#include "cleanup/ExtraRetentionInternal.hpp"
#include <memory>

namespace batbox::cleanup {
namespace {
struct SingleSweep:std::enable_shared_from_this<SingleSweep>{
  event_loop::Loop& loop;ExtraServices services;
  event_loop::Deferred<Result> result;Result counts;
  std::string dir,extension;bool removeEmpty;
  double cutoff=0;std::vector<Dirent> files;std::size_t index=0;
  SingleSweep(event_loop::Loop& owner,std::string path,std::string suffix,bool remove,
    ExtraServices imports):loop(owner),services(std::move(imports)),result(owner),
    dir(std::move(path)),extension(std::move(suffix)),removeEmpty(remove){}
  void start(){
    cutoff=services.getCutoffDateMs();auto self=shared_from_this();
    services.fs.readdir(dir).then([self](const std::vector<Dirent>* value,
      const std::exception_ptr& error){
      if(error){self->result.resolve(self->counts);return;}
      self->files=*value;self->next();
    });
  }
  void next(){
    while(index<files.size()&&(!files[index].isFile||
      !files[index].name.ends_with(extension)))++index;
    if(index>=files.size()){
      if(!removeEmpty){result.resolve(counts);return;}
      auto self=shared_from_this();
      detail::tryRmdir(loop,dir,services).then([self](const event_loop::Unit*,
        const std::exception_ptr&){self->result.resolve(self->counts);});
      return;
    }
    const auto file=services.joinPath(dir,files[index++].name);
    auto self=shared_from_this();
    detail::unlinkIfOld(loop,file,cutoff,services).then([self](const bool* removed,
      const std::exception_ptr& error){
      if(error)++self->counts.errors;
      else if(*removed)++self->counts.messages;
      self->next();
    });
  }
};
struct DirectorySweep:std::enable_shared_from_this<DirectorySweep>{
  event_loop::Loop& loop;ExtraServices services;
  event_loop::Deferred<Result> result;Result counts;
  std::string dir;double cutoff=0;
  std::vector<std::string> directories;std::size_t index=0;
  bool parallel=false;
  DirectorySweep(event_loop::Loop& owner,std::string path,bool concurrent,
    ExtraServices imports):loop(owner),services(std::move(imports)),result(owner),
    dir(std::move(path)),parallel(concurrent){}
  void start(){
    auto self=shared_from_this();
    services.fs.readdir(dir).then([self](const std::vector<Dirent>* entries,
      const std::exception_ptr& error){
      if(error){self->result.resolve(self->counts);return;}
      for(const auto& entry:*entries)if(entry.isDirectory)
        self->directories.push_back(self->services.joinPath(self->dir,entry.name));
      if(self->parallel)self->startParallel();else self->next();
    });
  }
  void removeIfOld(const std::string& path,std::function<void()> done){
    auto self=shared_from_this();
    services.fs.statMtimeMs(path).then([self,path,done](const double* mtime,
      const std::exception_ptr& error){
      if(error){++self->counts.errors;done();return;}
      if(!(*mtime<self->cutoff)){done();return;}
      self->services.fs.rmRecursiveForce(path).then([self,done](const event_loop::Unit*,
        const std::exception_ptr& removeError){
        if(removeError)++self->counts.errors;
        else ++self->counts.messages;
        done();
      });
    });
  }
  void finish(){
    auto self=shared_from_this();
    detail::tryRmdir(loop,dir,services).then([self](const event_loop::Unit*,
      const std::exception_ptr&){self->result.resolve(self->counts);});
  }
  void next(){
    if(index>=directories.size()){finish();return;}
    const auto path=directories[index++];auto self=shared_from_this();
    removeIfOld(path,[self]{self->next();});
  }
  void startParallel(){
    if(directories.empty()){finish();return;}
    auto self=shared_from_this();
    auto remaining=std::make_shared<std::size_t>(directories.size());
    for(const auto& path:directories){
      removeIfOld(path,[self,remaining]{if(--*remaining==0)self->finish();});
    }
  }
};
struct DebugSweep:std::enable_shared_from_this<DebugSweep>{
  event_loop::Loop& loop;ExtraServices services;
  event_loop::Deferred<Result> result;Result counts;
  std::string dir;double cutoff=0;
  std::vector<Dirent> files;std::size_t index=0;
  DebugSweep(event_loop::Loop& owner,ExtraServices imports):loop(owner),
    services(std::move(imports)),result(owner){}
  void start(){
    cutoff=services.getCutoffDateMs();
    dir=services.joinPath(services.getConfigHome(),"debug");
    auto self=shared_from_this();
    services.fs.readdir(dir).then([self](const std::vector<Dirent>* entries,
      const std::exception_ptr& error){
      if(error){self->result.resolve(self->counts);return;}
      self->files=*entries;self->next();
    });
  }
  void next(){
    while(index<files.size()&&(!files[index].isFile||
      !files[index].name.ends_with(".txt")||files[index].name=="latest"))++index;
    if(index>=files.size()){result.resolve(counts);return;}
    const auto path=services.joinPath(dir,files[index++].name);
    auto self=shared_from_this();
    detail::unlinkIfOld(loop,path,cutoff,services).then([self](const bool* removed,
      const std::exception_ptr& error){
      if(error)++self->counts.errors;
      else if(*removed)++self->counts.messages;
      self->next();
    });
  }
};
event_loop::Deferred<Result> directorySweep(event_loop::Loop& loop,
  const ExtraServices& services,const std::string& suffix,bool parallel){
  event_loop::Deferred<Result> fallback(loop);
  double cutoff;
  try{cutoff=services.getCutoffDateMs();}
  catch(...){fallback.reject(std::current_exception());return fallback;}
  try{
    const auto dir=services.joinPath(services.getConfigHome(),suffix);
    auto sweep=std::make_shared<DirectorySweep>(loop,dir,parallel,services);
    sweep->cutoff=cutoff;sweep->start();return sweep->result;
  }catch(...){
    try{services.logError(std::current_exception());fallback.resolve({});}
    catch(...){fallback.reject(std::current_exception());}
    return fallback;
  }
}
}
event_loop::Deferred<Result> cleanupSingleDirectory(event_loop::Loop& loop,
  const std::string& dir,const std::string& extension,bool removeEmpty,
  const ExtraServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("cleanup requires loop owner");
  detail::require(services);
  auto sweep=std::make_shared<SingleSweep>(loop,dir,extension,removeEmpty,services);
  try{sweep->start();}catch(...){sweep->result.reject(std::current_exception());}
  return sweep->result;
}
event_loop::Deferred<Result> cleanupOldPlanFiles(event_loop::Loop& loop,
  const ExtraServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("cleanup requires loop owner");
  detail::require(services);
  const auto dir=services.joinPath(services.getConfigHome(),"plans");
  return cleanupSingleDirectory(loop,dir,".md",true,services);
}
event_loop::Deferred<Result> cleanupOldFileHistoryBackups(event_loop::Loop& loop,
  const ExtraServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("cleanup requires loop owner");
  detail::require(services);
  return directorySweep(loop,services,"file-history",true);
}
event_loop::Deferred<Result> cleanupOldFlightEnvDirs(event_loop::Loop& loop,
  const ExtraServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("cleanup requires loop owner");
  detail::require(services);
  return directorySweep(loop,services,"session-env",false);
}
event_loop::Deferred<Result> cleanupOldDebugLogs(event_loop::Loop& loop,
  const ExtraServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("cleanup requires loop owner");
  detail::require(services);
  auto sweep=std::make_shared<DebugSweep>(loop,services);
  try{sweep->start();}catch(...){sweep->result.reject(std::current_exception());}
  return sweep->result;
}
} // namespace batbox::cleanup
