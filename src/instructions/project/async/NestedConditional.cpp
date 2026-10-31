#include "instructions/project/async/AsyncDiscovery.hpp"
#include "shell/PosixPath.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::instructions::project::async {
namespace {
std::string join(const std::string& left,const std::string& right){
  return shell::joinPosixPath(left,right);
}
struct NestedRun:std::enable_shared_from_this<NestedRun> {
  event_loop::Loop& loop;
  std::string dir,target,rulesDir;
  std::shared_ptr<PathSet> processed,unconditional;
  Services services;
  Deferred<MemoryFiles> output;
  MemoryFiles result;
  NestedRun(event_loop::Loop& owner,std::string directory,std::string targetPath,
    std::shared_ptr<PathSet> paths,Services imports):loop(owner),
    dir(std::move(directory)),target(std::move(targetPath)),
    processed(std::move(paths)),services(std::move(imports)),output(owner){}
  void append(Deferred<MemoryFiles> pending,std::function<void()> next){
    pending.then([self=shared_from_this(),next=std::move(next)](
      const MemoryFiles* files,const std::exception_ptr& error){
      if(error){self->output.reject(error);return;}
      try{self->result.insert(self->result.end(),files->begin(),files->end());next();}
      catch(...){self->output.reject(std::current_exception());}
    });
  }
  void start(){
    try{
      if(!services.sync.settingSourceEnabled)
        throw std::invalid_argument("Setting-source import required");
      if(services.sync.settingSourceEnabled("projectSettings")){
        append(processMemoryFile(loop,join(dir,"BATBOX.md"),"Project",processed,
          false,services),[self=shared_from_this()]{self->dotFile();});
      }else localFile();
    }catch(...){output.reject(std::current_exception());}
  }
  void dotFile(){
    try{append(processMemoryFile(loop,join(join(dir,".batbox"),"BATBOX.md"),
      "Project",processed,false,services),
      [self=shared_from_this()]{self->localFile();});}
    catch(...){output.reject(std::current_exception());}
  }
  void localFile(){
    try{
      if(services.sync.settingSourceEnabled("localSettings")){
        append(processMemoryFile(loop,join(dir,"BATBOX.local.md"),"Local",processed,
          false,services),[self=shared_from_this()]{self->unconditionalRules();});
      }else unconditionalRules();
    }catch(...){output.reject(std::current_exception());}
  }
  void unconditionalRules(){
    try{
      rulesDir=join(join(dir,".batbox"),"rules");
      unconditional=std::make_shared<PathSet>(*processed);
      append(processMdRules(loop,rulesDir,"Project",unconditional,false,false,
        services),[self=shared_from_this()]{self->conditionalRules();});
    }catch(...){output.reject(std::current_exception());}
  }
  void conditionalRules(){
    try{
      append(processConditionedMdRules(loop,target,rulesDir,"Project",processed,
        false,services),[self=shared_from_this()]{
        try{
          self->processed->insert(self->unconditional->begin(),
            self->unconditional->end());
          self->output.resolve(std::move(self->result));
        }catch(...){self->output.reject(std::current_exception());}
      });
    }catch(...){output.reject(std::current_exception());}
  }
};
}
Deferred<MemoryFiles> getMemoryFilesForNestedDirectory(event_loop::Loop& loop,
  const std::string& dir,const std::string& targetPath,
  std::shared_ptr<PathSet> processedPaths,const Services& services){
  if(!loop.isOwnerThread())throw std::logic_error("Nested instruction owner required");
  Deferred<MemoryFiles> output(loop);
  if(!processedPaths){output.reject(std::make_exception_ptr(std::invalid_argument(
    "Processed instruction paths required")));return output;}
  auto run=std::make_shared<NestedRun>(loop,dir,targetPath,
    std::move(processedPaths),services);
  run->start();return run->output;
}
} // namespace batbox::instructions::project::async
