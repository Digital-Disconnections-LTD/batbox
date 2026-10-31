#include "locking/Bindings.hpp"

namespace batbox::locking {
CleanupCallbacks makeCleanupCallbacks(event_loop::Loop& loop,
    Manager& manager){
  Options options;
  options.realpath=false;
  options.retry.retries=0;
  CleanupCallbacks callbacks;
  callbacks.lock=[&loop,&manager,options](const std::string& path){
    event_loop::Deferred<event_loop::Unit> output(loop);
    manager.lock(path,options).then([output](const Release*,
        const std::exception_ptr& error){
      if(error)output.reject(error);else output.resolve({});
    });
    return output;
  };
  callbacks.unlock=[&manager,options](const std::string& path){
    return manager.unlock(path,options);
  };
  return callbacks;
}
}
