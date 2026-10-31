#include "locking/Core.hpp"
#include "locking/Async.hpp"
#include <cerrno>
#include <system_error>

namespace batbox::locking {
namespace {
enum class UpdateStatus {Success,StatError,MtimeChanged,UtimeError};
struct UpdateResult {
  UpdateStatus status=UpdateStatus::Success;
  int errorCode=0;
  double mtimeMs=0;
  bool overThreshold=false;
};
struct UpdateSnapshot {
  std::string lockPath;
  double mtimeMs,lastUpdateMs,staleMs;
  file_ops::Precision precision;
};
UpdateResult update(const UpdateSnapshot& owned){
  UpdateResult result;
  try{result.mtimeMs=file_ops::statMtime(owned.lockPath);}
  catch(const std::system_error& error){
    result.status=UpdateStatus::StatError;
    result.errorCode=error.code().value();
    result.overThreshold=owned.lastUpdateMs+
      owned.staleMs<file_ops::nowMs();
    return result;
  }
  result.overThreshold=owned.lastUpdateMs+
    owned.staleMs<file_ops::nowMs();
  if(result.mtimeMs!=owned.mtimeMs){
    result.status=UpdateStatus::MtimeChanged;return result;
  }
  result.mtimeMs=file_ops::heartbeatMtime(owned.precision);
  try{file_ops::setMtime(owned.lockPath,result.mtimeMs);}
  catch(const std::system_error& error){
    result.status=UpdateStatus::UtimeError;
    result.errorCode=error.code().value();
    result.overThreshold=owned.lastUpdateMs+
      owned.staleMs<file_ops::nowMs();
  }
  return result;
}
}
void Core::schedule(const std::shared_ptr<Owned>& owned){
  if(owned->released||!alive||owned->timer.valid())return;
  const double delay=owned->updateDelay.value_or(*owned->options.updateMs);
  auto self=shared_from_this();
  owned->timer=loop.setTimeout(delay,[self,owned]{
    owned->timer={};
    if(owned->released||!self->alive)return;
    const UpdateSnapshot snapshot{owned->lockPath,owned->mtimeMs,
      owned->lastUpdateMs,owned->options.staleMs,owned->precision};
    runJob<UpdateResult>(self->loop,[snapshot]{return update(snapshot);})
      .then([self,owned](const UpdateResult* result,
          const std::exception_ptr& error){
        if(owned->released||!self->alive)return;
        if(error){
          self->compromise(owned,Error("ECOMPROMISED",
            "Unable to update lock within the stale threshold"));
          return;
        }
        if(result->status==UpdateStatus::StatError||
            result->status==UpdateStatus::UtimeError){
          if(result->errorCode==ENOENT||result->overThreshold){
            self->compromise(owned,Error("ECOMPROMISED",
              "Directory lock heartbeat failed"));
          }else{
            owned->updateDelay=1000;
            self->schedule(owned);
          }
          return;
        }
        if(result->status==UpdateStatus::MtimeChanged){
          self->compromise(owned,Error("ECOMPROMISED",
            "Unable to update lock within the stale threshold"));
          return;
        }
        owned->mtimeMs=result->mtimeMs;
        owned->lastUpdateMs=file_ops::nowMs();
        owned->updateDelay.reset();
        self->schedule(owned);
      });
  });
  owned->timer.unref();
}
void Core::compromise(const std::shared_ptr<Owned>& owned,
    const Error& error){
  owned->released=true;
  owned->timer.cancel();
  if(const auto found=locks.find(owned->canonical);
      found!=locks.end()&&found->second==owned)locks.erase(found);
  if(owned->options.onCompromised)
    owned->options.onCompromised(error);
  else throw error;
}
}
