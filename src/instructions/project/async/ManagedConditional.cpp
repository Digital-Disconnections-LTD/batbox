#include "instructions/project/async/AsyncDiscovery.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::instructions::project::async {
namespace {
struct ManagedRun:std::enable_shared_from_this<ManagedRun> {
  event_loop::Loop& loop;
  std::string target;
  std::shared_ptr<PathSet> processed;
  Services services;
  Deferred<MemoryFiles> output;
  MemoryFiles result;
  ManagedRun(event_loop::Loop& owner,std::string path,
    std::shared_ptr<PathSet> paths,Services imports):loop(owner),
    target(std::move(path)),processed(std::move(paths)),
    services(std::move(imports)),output(owner){}
  void start(){
    try{
      if(!services.sync.managedRulesDir)
        throw std::invalid_argument("Managed rule path import required");
      processConditionedMdRules(loop,target,services.sync.managedRulesDir(),
        "Managed",processed,false,services).then([self=shared_from_this()](
        const MemoryFiles* files,const std::exception_ptr& error){
        if(error){self->output.reject(error);return;}
        try{
          self->result.insert(self->result.end(),files->begin(),files->end());
          self->user();
        }catch(...){self->output.reject(std::current_exception());}
      });
    }catch(...){output.reject(std::current_exception());}
  }
  void user(){
    try{
      if(!services.sync.settingSourceEnabled)
        throw std::invalid_argument("Setting-source import required");
      if(!services.sync.settingSourceEnabled("userSettings")){
        output.resolve(std::move(result));return;}
      if(!services.sync.userRulesDir)
        throw std::invalid_argument("User rule path import required");
      processConditionedMdRules(loop,target,services.sync.userRulesDir(),
        "User",processed,true,services).then([self=shared_from_this()](
        const MemoryFiles* files,const std::exception_ptr& error){
        if(error){self->output.reject(error);return;}
        try{self->result.insert(self->result.end(),files->begin(),files->end());
          self->output.resolve(std::move(self->result));}
        catch(...){self->output.reject(std::current_exception());}
      });
    }catch(...){output.reject(std::current_exception());}
  }
};
}
Deferred<MemoryFiles> getManagedAndUserConditionalRules(event_loop::Loop& loop,
  const std::string& targetPath,std::shared_ptr<PathSet> processedPaths,
  const Services& services){
  if(!loop.isOwnerThread())throw std::logic_error("Managed rule owner required");
  Deferred<MemoryFiles> output(loop);
  if(!processedPaths){output.reject(std::make_exception_ptr(std::invalid_argument(
    "Processed instruction paths required")));return output;}
  auto run=std::make_shared<ManagedRun>(loop,targetPath,std::move(processedPaths),services);
  run->start();return run->output;
}
} // namespace batbox::instructions::project::async
