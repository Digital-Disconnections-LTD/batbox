#include "auth/refresh/LockBinding.hpp"
#include "locking/Async.hpp"
#include "locking/FileOps.hpp"
#include "runtime/Encoding.hpp"
#include <filesystem>
#include <random>

namespace batbox::auth::refresh {
void bindIndigenousRefreshInfrastructure(CheckServices& services,
    event_loop::Loop& loop,locking::Manager& locks){
  services.nowMs=locking::file_ops::nowMs;
  services.random=[](){
    static thread_local std::mt19937_64 random(std::random_device{}());
    return std::uniform_real_distribution<double>(0,1)(random);
  };
  services.mkdirRecursive=[&loop](const std::string& path){
    return locking::runJob<event_loop::Unit>(loop,[path]{
      std::filesystem::create_directories(
        std::filesystem::path(value::encodeUtf8(path)));
      return event_loop::Unit{};
    });
  };
  services.sleep=[&loop](double delay){
    event_loop::Deferred<event_loop::Unit> output(loop);
    loop.setTimeout(delay,[output](){output.resolve({});});
    return output;
  };
  services.lockDirectory=[&loop,&locks](const std::string& path){
    event_loop::Deferred<ReleaseLock> output(loop);
    locks.lock(path).then([output](const locking::Release* release,
        const std::exception_ptr& error){
      if(error){
        try{std::rethrow_exception(error);}
        catch(const locking::Error& lockError){
          output.reject(std::make_exception_ptr(
            LockError(lockError.code,lockError.what())));
        }catch(...){output.reject(error);}
      }else output.resolve(*release);
    });
    return output;
  };
}
}
