#include "cron/CronLock.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace batbox::cron {
namespace {
bool errnoIs(const std::exception_ptr& error,int code){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::system_error& ex){return ex.code().value()==code;}
  catch(...){}
  return false;
}
std::string body(const SchedulerLockRecord& lock){
  Json value=Json::object();value["sessionId"]=lock.flightId;
  value["pid"]=lock.pid;value["acquiredAt"]=lock.acquiredAt;
  return value::stringify(value);
}
}
CronLock::CronLock(event_loop::Loop& loop,CronLockServices services):
  loop_(loop),services_(std::move(services)){
  if(!services_.getProjectRoot||!services_.getFlightId||
     !services_.nowMs||!services_.pid||!services_.isProcessRunning||
     !services_.readFile||!services_.writeFile||!services_.mkdir||
     !services_.unlink||!services_.registerCleanup||!services_.logDebug)
    throw std::invalid_argument("Cron lock bindings required");
}
std::string CronLock::getLockPath(const SchedulerLockOptions& options)const{
  const auto root=options.dir?*options.dir:services_.getProjectRoot();
  return (std::filesystem::path(root)/".batbox"/
    "scheduled_tasks.lock").lexically_normal().string();
}
event_loop::Deferred<std::optional<SchedulerLockRecord>> CronLock::readLock(
  const SchedulerLockOptions& options){
  event_loop::Deferred<std::optional<SchedulerLockRecord>> output(loop_);
  try{
    auto read=services_.readFile(getLockPath(options));
    read.then([output](const std::string* raw,
      const std::exception_ptr& error){
      if(error){output.resolve(std::nullopt);return;}
      try{
        const auto parsed=value::parse(*raw,false);
        if(!parsed.is_object()||!parsed.contains("sessionId")||
           !parsed["sessionId"].is_string()||!parsed.contains("pid")||
           !parsed["pid"].is_number()||
           !parsed.contains("acquiredAt")||
           !parsed["acquiredAt"].is_number()){
          output.resolve(std::nullopt);return;
        }
        output.resolve(SchedulerLockRecord{
          parsed["sessionId"].get<std::string>(),
          parsed["pid"].get<double>(),parsed["acquiredAt"].get<double>()});
      }catch(...){output.resolve(std::nullopt);}
    });
  }catch(...){output.resolve(std::nullopt);}
  return output;
}
event_loop::Deferred<bool> CronLock::tryCreateExclusive(
  const SchedulerLockRecord& record,
  const SchedulerLockOptions& options){
  event_loop::Deferred<bool> output(loop_);
  try{
    const auto path=getLockPath(options),serialized=body(record);
    auto first=services_.writeFile(path,serialized,true);
    const auto self=shared_from_this();
    first.then([self,output,path,serialized](const event_loop::Unit*,
      const std::exception_ptr& error){
      if(!error){output.resolve(true);return;}
      if(errnoIs(error,EEXIST)){output.resolve(false);return;}
      if(!errnoIs(error,ENOENT)){output.reject(error);return;}
      try{
        auto created=self->services_.mkdir(
          std::filesystem::path(path).parent_path().string());
        created.then([self,output,path,serialized](const event_loop::Unit*,
          const std::exception_ptr& mkdirError){
          if(mkdirError){output.reject(mkdirError);return;}
          try{
            auto second=self->services_.writeFile(path,serialized,true);
            second.then([output](const event_loop::Unit*,
              const std::exception_ptr& writeError){
              if(!writeError)output.resolve(true);
              else if(errnoIs(writeError,EEXIST))output.resolve(false);
              else output.reject(writeError);
            });
          }catch(...){output.reject(std::current_exception());}
        });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void CronLock::registerLockCleanup(const SchedulerLockOptions& options){
  if(unregisterCleanup_)unregisterCleanup_();
  std::weak_ptr<CronLock> weak=shared_from_this();
  unregisterCleanup_=services_.registerCleanup([weak,options]{
    if(const auto self=weak.lock())return self->release(options);
    throw std::logic_error("Cron lock owner unavailable during cleanup");
  });
}
event_loop::Deferred<bool> CronLock::tryAcquire(
  const SchedulerLockOptions& options){
  event_loop::Deferred<bool> output(loop_);
  try{
    const SchedulerLockRecord record{
      options.lockIdentity?*options.lockIdentity:services_.getFlightId(),
      services_.pid(),services_.nowMs()};
    const auto self=shared_from_this();
    auto created=tryCreateExclusive(record,options);
    created.then([self,output,record,options](const bool* success,
      const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      if(*success){
        try{
          self->lastBlockedBy_.reset();
          self->registerLockCleanup(options);
          self->services_.logDebug(
            "[ScheduledTasks] acquired scheduler lock (PID "+
            value::numberToString(record.pid)+")");
          output.resolve(true);
        }catch(...){output.reject(std::current_exception());}
        return;
      }
      try{
        auto read=self->readLock(options);
        read.then([self,output,record,options](
          const std::optional<SchedulerLockRecord>* existing,
          const std::exception_ptr& readError){
          if(readError){output.reject(readError);return;}
          try{
            if(*existing&&existing->value().flightId==record.flightId){
              if(existing->value().pid!=record.pid){
                auto write=self->services_.writeFile(
                  self->getLockPath(options),body(record),false);
                write.then([self,output,options](const event_loop::Unit*,
                  const std::exception_ptr& writeError){
                  if(writeError){output.reject(writeError);return;}
                  try{self->registerLockCleanup(options);
                    output.resolve(true);}
                  catch(...){output.reject(std::current_exception());}
                });
              }else output.resolve(true);
              return;
            }
            if(*existing&&self->services_.isProcessRunning(
                existing->value().pid)){
              if(self->lastBlockedBy_!=existing->value().flightId){
                self->lastBlockedBy_=existing->value().flightId;
                self->services_.logDebug(
                  "[ScheduledTasks] scheduler lock held by session "+
                  existing->value().flightId+" (PID "+
                  value::numberToString(existing->value().pid)+")");
              }
              output.resolve(false);return;
            }
            if(*existing)self->services_.logDebug(
              "[ScheduledTasks] recovering stale scheduler lock from PID "+
              value::numberToString(existing->value().pid));
            auto unlinked=self->services_.unlink(
              self->getLockPath(options));
            unlinked.then([self,output,record,options](
              const event_loop::Unit*,const std::exception_ptr&){
              try{
                auto recovered=self->tryCreateExclusive(record,options);
                recovered.then([self,output,options](const bool* own,
                  const std::exception_ptr& recoverError){
                  if(recoverError){output.reject(recoverError);return;}
                  try{
                    if(*own){self->lastBlockedBy_.reset();
                      self->registerLockCleanup(options);}
                    output.resolve(*own);
                  }catch(...){output.reject(std::current_exception());}
                });
              }catch(...){output.reject(std::current_exception());}
            });
          }catch(...){output.reject(std::current_exception());}
        });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<event_loop::Unit> CronLock::release(
  const SchedulerLockOptions& options){
  event_loop::Deferred<event_loop::Unit> output(loop_);
  try{
    if(unregisterCleanup_){unregisterCleanup_();unregisterCleanup_={};}
    lastBlockedBy_.reset();
    const auto identity=options.lockIdentity?*options.lockIdentity:
      services_.getFlightId();
    const auto self=shared_from_this();
    auto read=readLock(options);
    read.then([self,output,options,identity](
      const std::optional<SchedulerLockRecord>* existing,
      const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      if(!*existing||existing->value().flightId!=identity){
        output.resolve({});return;
      }
      try{
        auto removed=self->services_.unlink(self->getLockPath(options));
        removed.then([self,output](const event_loop::Unit*,
          const std::exception_ptr& unlinkError){
          try{
            if(!unlinkError)self->services_.logDebug(
              "[ScheduledTasks] released scheduler lock");
            output.resolve({});
          }catch(...){output.reject(std::current_exception());}
        });
      }catch(...){output.resolve({});}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
