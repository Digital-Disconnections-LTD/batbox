#include "cleanup/ExtraRetentionInternal.hpp"
#include <memory>

namespace batbox::cleanup {
namespace {
struct FlightSweep:std::enable_shared_from_this<FlightSweep>{
  event_loop::Loop& loop;
  ExtraServices services;
  event_loop::Deferred<Result> result;
  double cutoff=0;
  Result counts;
  std::string projectsDir,projectDir,flightDir,gadgetResultsDir,gadgetDir;
  std::vector<Dirent> projects,entries,gadgetEntries,gadgetFiles;
  std::size_t projectIndex=0,entryIndex=0,gadgetIndex=0,fileIndex=0;
  FlightSweep(event_loop::Loop& owner,ExtraServices imports):loop(owner),
    services(std::move(imports)),result(owner){}
  void start(){
    cutoff=services.getCutoffDateMs();
    projectsDir=services.getProjectsDir();
    auto self=shared_from_this();
    services.fs.readdir(projectsDir).then([self](const std::vector<Dirent>* value,
      const std::exception_ptr& error){
      if(error){self->result.resolve(self->counts);return;}
      self->projects=*value;self->nextProject();
    });
  }
  void nextProject(){
    while(projectIndex<projects.size()&&!projects[projectIndex].isDirectory)++projectIndex;
    if(projectIndex>=projects.size()){result.resolve(counts);return;}
    projectDir=services.joinPath(projectsDir,projects[projectIndex++].name);
    auto self=shared_from_this();
    services.fs.readdir(projectDir).then([self](const std::vector<Dirent>* value,
      const std::exception_ptr& error){
      if(error){++self->counts.errors;self->nextProject();return;}
      self->entries=*value;self->entryIndex=0;self->nextEntry();
    });
  }
  void nextEntry(){
    while(entryIndex<entries.size()){
      const auto& candidate=entries[entryIndex];
      if(candidate.isDirectory||
        (candidate.isFile&&(candidate.name.ends_with(".jsonl")||candidate.name.ends_with(".cast"))))break;
      ++entryIndex;
    }
    if(entryIndex>=entries.size()){
      auto self=shared_from_this();
      detail::tryRmdir(loop,projectDir,services).then([self](const event_loop::Unit*,
        const std::exception_ptr&){self->nextProject();});
      return;
    }
    const auto entry=entries[entryIndex++];
    if(entry.isFile){
      auto self=shared_from_this();
      detail::unlinkIfOld(loop,services.joinPath(projectDir,entry.name),cutoff,services).then(
        [self](const bool* removed,const std::exception_ptr& error){
          if(error)++self->counts.errors;
          else if(*removed)++self->counts.messages;
          self->nextEntry();
        });
      return;
    }
    flightDir=services.joinPath(projectDir,entry.name);
    gadgetResultsDir=services.joinPath(flightDir,services.gadgetResultsSubdir);
    auto self=shared_from_this();
    services.fs.readdir(gadgetResultsDir).then([self](const std::vector<Dirent>* value,
      const std::exception_ptr& error){
      if(error){detail::tryRmdir(self->loop,self->flightDir,self->services).then(
        [self](const event_loop::Unit*,const std::exception_ptr&){self->nextEntry();});
        return;}
      self->gadgetEntries=*value;self->gadgetIndex=0;self->nextGadgetEntry();
    });
  }
  void nextGadgetEntry(){
    while(gadgetIndex<gadgetEntries.size()&&!gadgetEntries[gadgetIndex].isFile&&
      !gadgetEntries[gadgetIndex].isDirectory)++gadgetIndex;
    if(gadgetIndex>=gadgetEntries.size()){
      auto self=shared_from_this();
      detail::tryRmdir(loop,gadgetResultsDir,services).then([self](const event_loop::Unit*,
        const std::exception_ptr&){
        detail::tryRmdir(self->loop,self->flightDir,self->services).then(
          [self](const event_loop::Unit*,const std::exception_ptr&){self->nextEntry();});
      });
      return;
    }
    const auto entry=gadgetEntries[gadgetIndex++];
    if(entry.isFile){
      auto self=shared_from_this();
      detail::unlinkIfOld(loop,services.joinPath(gadgetResultsDir,entry.name),cutoff,services).then(
        [self](const bool* removed,const std::exception_ptr& error){
          if(error)++self->counts.errors;
          else if(*removed)++self->counts.messages;
          self->nextGadgetEntry();
        });
      return;
    }
    gadgetDir=services.joinPath(gadgetResultsDir,entry.name);
    auto self=shared_from_this();
    services.fs.readdir(gadgetDir).then([self](const std::vector<Dirent>* value,
      const std::exception_ptr& error){
      if(error){self->nextGadgetEntry();return;}
      self->gadgetFiles=*value;self->fileIndex=0;self->nextGadgetFile();
    });
  }
  void nextGadgetFile(){
    while(fileIndex<gadgetFiles.size()&&!gadgetFiles[fileIndex].isFile)++fileIndex;
    if(fileIndex>=gadgetFiles.size()){
      auto self=shared_from_this();
      detail::tryRmdir(loop,gadgetDir,services).then([self](const event_loop::Unit*,
        const std::exception_ptr&){self->nextGadgetEntry();});
      return;
    }
    const auto path=services.joinPath(gadgetDir,gadgetFiles[fileIndex++].name);
    auto self=shared_from_this();
    detail::unlinkIfOld(loop,path,cutoff,services).then(
      [self](const bool* removed,const std::exception_ptr& error){
        if(error)++self->counts.errors;
        else if(*removed)++self->counts.messages;
        self->nextGadgetFile();
      });
  }
};
}
event_loop::Deferred<Result> cleanupOldFlightFiles(event_loop::Loop& loop,
  const ExtraServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("cleanup requires loop owner");
  detail::require(services);
  auto sweep=std::make_shared<FlightSweep>(loop,services);
  try{sweep->start();}catch(...){sweep->result.reject(std::current_exception());}
  return sweep->result;
}
} // namespace batbox::cleanup
