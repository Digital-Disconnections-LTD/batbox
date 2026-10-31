#include "cleanup/Background.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::cleanup {
namespace {
struct Run:std::enable_shared_from_this<Run>{
  event_loop::Loop& loop;BackgroundServices services;
  event_loop::Deferred<event_loop::Unit> result;
  int step=0;
  Run(event_loop::Loop& owner,BackgroundServices imports):loop(owner),
    services(std::move(imports)),result(owner){}
  void advance(){
    auto self=shared_from_this();
    try{
      if(step<6){
        const auto current=step++;
        const auto& action=current==0?services.oldMessageFiles:
          current==1?services.oldFlightFiles:
          current==2?services.oldPlanFiles:
          current==3?services.oldFileHistoryBackups:
          current==4?services.oldFlightEnvDirs:services.oldDebugLogs;
        action().then([self](const Result*,const std::exception_ptr& error){
          if(error)self->result.reject(error);else self->advance();
        });return;
      }
      if(step==6){++step;services.oldImageStashes().then([self](const event_loop::Unit*,
        const std::exception_ptr& error){
        if(error)self->result.reject(error);else self->advance();
      });return;}
      if(step==7){++step;const auto cutoff=services.getCutoffDateMs();
        services.oldPastes(cutoff).then([self](const event_loop::Unit*,
          const std::exception_ptr& error){
          if(error)self->result.reject(error);else self->advance();
        });return;}
      if(step==8){++step;const auto cutoff=services.getCutoffDateMs();
        services.staleBatWorktrees(cutoff).then([self](const int*,
          const std::exception_ptr& error){
          if(error)self->result.reject(error);else self->advance();
        });return;}
      if(services.userType&&*services.userType==services.internalUserType){
        services.packageStashCleanup().then([self](const event_loop::Unit*,
          const std::exception_ptr& error){
          if(error)self->result.reject(error);else self->result.resolve(event_loop::Unit{});
        });return;
      }
      result.resolve(event_loop::Unit{});
    }catch(...){result.reject(std::current_exception());}
  }
};
void require(const BackgroundServices& services){
  if(!services.settingsErrorCount||!services.rawSettingsContainsKey||!services.debug||
    !services.oldMessageFiles||!services.oldFlightFiles||!services.oldPlanFiles||
    !services.oldFileHistoryBackups||!services.oldFlightEnvDirs||
    !services.oldDebugLogs||!services.oldImageStashes||!services.oldPastes||
    !services.staleBatWorktrees||!services.getCutoffDateMs||
    !services.packageStashCleanup||services.internalUserType.empty())
    throw std::invalid_argument("background cleanup imports required");
}
}
event_loop::Deferred<event_loop::Unit> cleanupOldMessageFilesInBackground(
  event_loop::Loop& loop,const BackgroundServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("background cleanup requires loop owner");
  require(services);
  event_loop::Deferred<event_loop::Unit> skipped(loop);
  try{
    if(services.settingsErrorCount()>0&&
      services.rawSettingsContainsKey("cleanupPeriodDays")){
      services.debug("Skipping cleanup: settings have validation errors but cleanupPeriodDays "
        "was explicitly set. Fix settings errors to enable cleanup.");
      skipped.resolve(event_loop::Unit{});return skipped;
    }
    auto run=std::make_shared<Run>(loop,services);run->advance();return run->result;
  }catch(...){skipped.reject(std::current_exception());return skipped;}
}
} // namespace batbox::cleanup
