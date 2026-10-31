#include "instructions/project/async/MemoryRun.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::instructions::project::async {
namespace {
bool instructionType(const std::string& type){
  return type=="User"||type=="Project"||type=="Local"||type=="Managed";
}
}
Deferred<MemoryFiles> MemoryDiscoveryStash::getMemoryFiles(
  const Services& services){return load(std::nullopt,services);}
Deferred<MemoryFiles> MemoryDiscoveryStash::getMemoryFiles(
  bool forceIncludeExternal,const Services& services){
  return load(forceIncludeExternal,services);
}
Deferred<MemoryFiles> MemoryDiscoveryStash::load(
  std::optional<bool> forceArgument,const Services& services){
  if(!loop_.isOwnerThread())throw std::logic_error("Instruction cache owner required");
  auto& slot=!forceArgument?normal_:(*forceArgument?forced_:explicitFalse_);
  if(slot)return *slot;
  auto run=std::make_shared<MemoryRun>(loop_,hooks_,services,forceArgument.value_or(false));
  run->start();
  slot=run->output; // lodash stores the Promise after the async body reaches its first await.
  return *slot;
}
void MemoryDiscoveryStash::clearMemoryFileStashes(){
  if(!loop_.isOwnerThread())throw std::logic_error("Instruction cache owner required");
  normal_.reset();explicitFalse_.reset();forced_.reset();
}
void MemoryDiscoveryStash::resetGetMemoryFilesStash(const std::string& reason){
  if(!loop_.isOwnerThread())throw std::logic_error("Instruction cache owner required");
  hooks_->nextEagerLoadReason=reason;
  hooks_->shouldFireHook=true;
  clearMemoryFileStashes();
}
void finishInstructionsLoadedHooks(const std::shared_ptr<HookState>& hooks,
  const MemoryFiles& files,bool forceIncludeExternal,const Services& services){
  if(forceIncludeExternal)return;
  if(!hooks->shouldFireHook)return;
  hooks->shouldFireHook=false;
  const auto reason=hooks->nextEagerLoadReason;
  hooks->nextEagerLoadReason="session_start";
  if(!services.sync.hasInstructionsLoadedHook)
    throw std::invalid_argument("InstructionsLoaded presence import required");
  if(!services.sync.hasInstructionsLoadedHook())return;
  if(!services.sync.instructionsLoadedHook)
    throw std::invalid_argument("InstructionsLoaded hook import required");
  for(const auto& file:files){
    if(!instructionType(file.type))continue;
    try{services.sync.instructionsLoadedHook(file,file.parent?"include":reason);}
    catch(...){/* Launch the hook mission without awaiting its result. */}
  }
}
void MemoryRun::callFile(const std::string& path,const std::string& type,
  bool external,std::function<void()> next){
  try{
    processMemoryFile(loop,path,type,processed,external,services).then(
      [self=shared_from_this(),next=std::move(next)](const MemoryFiles* files,
        const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{self->result.insert(self->result.end(),files->begin(),files->end());next();}
        catch(...){self->fail(std::current_exception());}
      });
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::callRules(const std::string& path,const std::string& type,
  bool external,std::function<void()> next){
  try{
    processMdRules(loop,path,type,processed,external,false,services).then(
      [self=shared_from_this(),next=std::move(next)](const MemoryFiles* files,
        const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{self->result.insert(self->result.end(),files->begin(),files->end());next();}
        catch(...){self->fail(std::current_exception());}
      });
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::start(){
  try{
    if(!services.sync.externalIncludesApproved||!services.sync.memoryPath)
      throw std::invalid_argument("Instruction config/path imports required");
    const bool approved=services.sync.externalIncludesApproved();
    includeExternal=forceIncludeExternal||approved;
    const auto path=services.sync.memoryPath("Managed");
    callFile(path,"Managed",includeExternal,
      [self=shared_from_this()]{self->managedRules();});
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::managedRules(){
  try{
    if(!services.sync.managedRulesDir)
      throw std::invalid_argument("Managed rules path import required");
    callRules(services.sync.managedRulesDir(),"Managed",includeExternal,
      [self=shared_from_this()]{self->userFile();});
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::userFile(){
  try{
    if(!services.sync.settingSourceEnabled)
      throw std::invalid_argument("Setting-source import required");
    if(!services.sync.settingSourceEnabled("userSettings")){setupDirs();return;}
    callFile(services.sync.memoryPath("User"),"User",true,
      [self=shared_from_this()]{self->userRules();});
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::userRules(){
  try{
    if(!services.sync.userRulesDir)
      throw std::invalid_argument("User rules path import required");
    callRules(services.sync.userRulesDir(),"User",true,
      [self=shared_from_this()]{self->setupDirs();});
  }catch(...){fail(std::current_exception());}
}
void MemoryRun::finish(){
  try{finishInstructionsLoadedHooks(hooks,result,forceIncludeExternal,services);
    output.resolve(std::move(result));}
  catch(...){fail(std::current_exception());}
}
} // namespace batbox::instructions::project::async
