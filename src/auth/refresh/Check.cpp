#include "auth/refresh/Check.hpp"
#include "auth/OAuthScopes.hpp"
#include <memory>
#include <utility>

namespace batbox::auth::refresh {
namespace {
Json property(const Json& object,const char* key){
  return object.is_object()&&object.contains(key)?object[key]:Json(nullptr);
}
bool hasRefresh(const std::optional<Json>& token){
  return token&&value::truthy(property(*token,"refreshToken"));
}
std::optional<double> expiresAt(const Json& token){
  const auto value=property(token,"expiresAt");
  return value.is_number()?std::optional(value.get<double>()):std::nullopt;
}
std::optional<std::vector<std::string>> scopes(const Json& token){
  const auto value=property(token,"scopes");
  if(!value.is_array())return std::nullopt;
  std::vector<std::string> output;
  for(const auto& scope:value){
    if(!scope.is_string())return std::nullopt;
    output.push_back(scope.get<std::string>());
  }
  return output;
}
std::string errorMessage(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& caught){return caught.what();}
  catch(...){return "unknown error";}
}
bool lockedError(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const LockError& caught){return caught.code=="ELOCKED";}
  catch(...){return false;}
}
class Attempt:public std::enable_shared_from_this<Attempt> {
public:
  Attempt(event_loop::Loop& loop,CheckServices services,int retry,bool force):
    loop_(loop),services_(std::move(services)),retry_(retry),force_(force),
    output_(loop){}
  event_loop::Deferred<bool> start(){
    try{
      services_.invalidateDiskStash().then([self=shared_from_this()](
          const event_loop::Unit*,const std::exception_ptr& error){
        if(error){self->output_.reject(error);return;}
        self->afterInvalidate();
      });
    }catch(...){output_.reject(std::current_exception());}
    return output_;
  }
private:
  event_loop::Loop& loop_;
  CheckServices services_;
  int retry_;
  bool force_;
  event_loop::Deferred<bool> output_;
  std::optional<ReleaseLock> release_;
  void clearForRead(){
    services_.clearTokenMemo();services_.clearKeychainStash();
  }
  bool expired(const Json& token){
    if(!token.is_object()||!token.contains("expiresAt")){
      (void)services_.nowMs();return false;
    }
    return isOAuthTokenExpired(expiresAt(token),services_.nowMs);
  }
  bool usesAuth(const Json& token){
    return shouldUseBatboxAuth(scopes(token),services_.inferenceScope);
  }
  void afterInvalidate(){
    try{
      const auto tokens=services_.stashedTokens();
      if(!force_&&(!hasRefresh(tokens)||!expired(*tokens))){
        output_.resolve(false);return;
      }
      if(!hasRefresh(tokens)||!usesAuth(*tokens)){
        output_.resolve(false);return;
      }
      clearForRead();
      services_.readTokensAsync().then([self=shared_from_this()](
          const std::optional<Json>* value,const std::exception_ptr& error){
        if(error){self->output_.reject(error);return;}
        self->afterFresh(*value);
      });
    }catch(...){output_.reject(std::current_exception());}
  }
  void afterFresh(const std::optional<Json>& tokens){
    try{
      if(!hasRefresh(tokens)||!expired(*tokens)){
        output_.resolve(false);return;
      }
      const auto home=services_.configHome();
      services_.mkdirRecursive(home).then([self=shared_from_this(),home](
          const event_loop::Unit*,const std::exception_ptr& error){
        if(error){self->output_.reject(error);return;}
        self->acquire(home);
      });
    }catch(...){output_.reject(std::current_exception());}
  }
  void acquire(const std::string& home){
    try{
      services_.lockDirectory(home).then([self=shared_from_this()](
          const ReleaseLock* release,const std::exception_ptr& error){
        if(error){self->afterLockError(error);return;}
        self->release_=*release;
        self->afterLock();
      });
    }catch(...){afterLockError(std::current_exception());}
  }
  void afterLockError(const std::exception_ptr& error){
    try{
      if(lockedError(error)){
        if(retry_<5){
          const auto delay=1000+services_.random()*1000;
          services_.sleep(delay).then([self=shared_from_this()](
              const event_loop::Unit*,const std::exception_ptr& sleepError){
            if(sleepError){self->output_.reject(sleepError);return;}
            auto next=std::make_shared<Attempt>(self->loop_,self->services_,
              self->retry_+1,self->force_);
            next->start().then([self](const bool* value,
                const std::exception_ptr& nextError){
              if(nextError)self->output_.reject(nextError);
              else self->output_.resolve(*value);
            });
          });
        }else output_.resolve(false);
        return;
      }
      services_.logError(errorMessage(error));
      output_.resolve(false);
    }catch(...){output_.reject(std::current_exception());}
  }
  void afterLock(){
    try{
      clearForRead();
      services_.readTokensAsync().then([self=shared_from_this()](
          const std::optional<Json>* tokens,const std::exception_ptr& error){
        if(error){self->recover(error);return;}
        self->afterLockedTokens(*tokens);
      });
    }catch(...){recover(std::current_exception());}
  }
  void afterLockedTokens(const std::optional<Json>& tokens){
    try{
      if(!hasRefresh(tokens)||!expired(*tokens)){
        finishWithRelease(false,{});return;
      }
      auto requested=usesAuth(*tokens)?std::optional<std::vector<std::string>>{}:
        scopes(*tokens);
      services_.refreshToken(property(*tokens,"refreshToken").get<std::string>(),
          requested).then([self=shared_from_this()](
          const Json* refreshed,const std::exception_ptr& error){
        if(error){self->recover(error);return;}
        try{
          self->services_.saveTokens(*refreshed);
          self->clearForRead();
          self->finishWithRelease(true,{});
        }catch(...){self->recover(std::current_exception());}
      });
    }catch(...){recover(std::current_exception());}
  }
  void recover(const std::exception_ptr& error){
    try{
      services_.logError(errorMessage(error));
      clearForRead();
      services_.readTokensAsync().then([self=shared_from_this()](
          const std::optional<Json>* current,
          const std::exception_ptr& readError){
        if(readError){self->finishWithRelease(false,readError);return;}
        try{
          const bool recovered=*current&&value::truthy(**current)&&
            !self->expired(**current);
          self->finishWithRelease(recovered,{});
        }catch(...){self->finishWithRelease(false,
          std::current_exception());}
      });
    }catch(...){finishWithRelease(false,std::current_exception());}
  }
  void finishWithRelease(bool value,const std::exception_ptr& error){
    if(!release_){
      output_.reject(std::make_exception_ptr(
        std::logic_error("Missing OAuth lock release")));return;
    }
    try{
      (*release_)().then([output=output_,value,error](
          const event_loop::Unit*,const std::exception_ptr& releaseError){
        if(releaseError)output.reject(releaseError);
        else if(error)output.reject(error);
        else output.resolve(value);
      });
    }catch(...){output_.reject(std::current_exception());}
  }
};
}
CheckController::CheckController(event_loop::Loop& loop,CheckServices services):
    loop_(loop),services_(std::move(services)){
  if(!services_.invalidateDiskStash||!services_.stashedTokens||
      !services_.clearTokenMemo||!services_.clearKeychainStash||
      !services_.readTokensAsync||!services_.nowMs||!services_.random||
      !services_.configHome||!services_.mkdirRecursive||
      !services_.lockDirectory||!services_.sleep||
      !services_.refreshToken||!services_.saveTokens||!services_.logError||
      services_.inferenceScope.empty())
    throw std::invalid_argument("Missing OAuth refresh-check binding");
}
event_loop::Deferred<bool> CheckController::attempt(
    int retryCount,bool force){
  return std::make_shared<Attempt>(loop_,services_,retryCount,force)->start();
}
event_loop::Deferred<bool> CheckController::check(int retryCount,bool force){
  if(retryCount==0&&!force){
    if(pending_)return *pending_;
    event_loop::Deferred<bool> output(loop_);
    attempt(retryCount,force).then([this,output](
        const bool* value,const std::exception_ptr& error){
      pending_.reset();
      if(error)output.reject(error);
      else output.resolve(*value);
    });
    pending_=output;return output;
  }
  return attempt(retryCount,force);
}
}
