#include "application_poetry/Provider.hpp"
#include "application_poetry/Stamp.hpp"
#include "project_recovery/Recovery.hpp"
#include "runtime/IndigenousPath.hpp"
#include "utils/envUtils.hpp"
#include <filesystem>

namespace batbox::application_poetry {

event_loop::Deferred<std::string> Provider::projectRecovery(){
  if(!owner_.isOwnerThread())
    throw std::logic_error("Project recovery requires owner loop");
  event_loop::Deferred<std::string> output(owner_);
  const auto currentSettings=settings();
  if(isEnvTruthy(value::property(inputs_.environment,"BATBOX_DISABLE_BATBOX_MDS"))||
      value::truthy(value::property(currentSettings,"skipProjectInstructions"))||
      value::truthy(value::property(currentSettings,"bareMode"))){
    output.resolve({});return output;
  }
  // A stable head keeps the packet it was built with, including no packet;
  // changes to the three startup files reach the model at the tail.
  const bool stable=stablePromptPrefix();
  if(stable&&projectRecoveryStash_)return *projectRecoveryStash_;
  const auto working=runtime::pathFromUtf8(cwd());
  const auto directory=project_recovery::findDirectory(working);
  if(!directory){
    projectRecoveryStamp_.clear();
    if(stable)projectRecoveryStash_=output;else projectRecoveryStash_.reset();
    output.resolve({});return output;
  }
  // Directory metadata detects newly added topic filenames; topic contents
  // remain on demand. Only the three startup files affect the loaded packet.
  auto signature=runtime::pathToUtf8(*directory)+"\n"+fileStamp(*directory);
  for(const char* name:{"Makefile","00_project.md","10_memory.md"}){
    signature+="\n"+fileStamp(*directory/name);
    if(stable)trackHeadFile(runtime::pathToUtf8(*directory/name));
  }
  if(projectRecoveryStash_&&signature==projectRecoveryStamp_)
    return *projectRecoveryStash_;
  projectRecoveryStamp_=signature;projectRecoveryStash_=output;
  auto self=shared_from_this();
  const auto endActivity=[self]{
    if(self->activeRecoveryLoads_&&--self->activeRecoveryLoads_==0&&
        self->inputs_.recoveryActivity)self->inputs_.recoveryActivity(false);
  };
  auto packet=std::make_shared<project_recovery::RecoveryPacket>();
  ++activeRecoveryLoads_;
  try{
    if(activeRecoveryLoads_==1&&inputs_.recoveryActivity)
      inputs_.recoveryActivity(true);
    owner_.queueWork([packet,working]{*packet=project_recovery::load(working);},
      [self,packet,output,endActivity,signature](std::exception_ptr error){
        try{
          endActivity();
          if(error)std::rethrow_exception(error);
          if(self->inputs_.logDiagnostic)for(const auto& warning:packet->warnings)
            self->inputs_.logDiagnostic("project-recovery",warning,std::nullopt);
          output.resolve(std::move(packet->text));
        }catch(...){
          if(self->projectRecoveryStamp_==signature)self->projectRecoveryStash_.reset();
          output.reject(std::current_exception());
        }
      });
  }catch(...){
    const auto error=std::current_exception();
    try{endActivity();}catch(...){}
    projectRecoveryStash_.reset();output.reject(error);
  }
  return output;
}
} // namespace batbox::application_poetry
