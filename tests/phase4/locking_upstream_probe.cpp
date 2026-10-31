#include "locking/ProperLock.hpp"
#include "locking/FileOps.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

template<class T>T await(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<T> promise){
  std::optional<T> value;std::exception_ptr failure;
  promise.then([&](const T* result,const std::exception_ptr& error){
    if(error)failure=error;else value=*result;
  });
  loop.run();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("Lock did not settle");
  return *value;
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    char temporary[]="/tmp/batbox-lock-probe-XXXXXX";
    const char* root=::mkdtemp(temporary);
    if(!root)throw std::runtime_error("Cannot make lock fixture dir");
    const auto file=test.value("missing",false)
      ?std::string(root)+"/missing":std::string(root);
    event_loop::Loop loop;
    locking::Manager locks(loop);
    locking::Options options;
    options.realpath=!test.value("missing",false);
    if(test.contains("stale"))options.staleMs=test["stale"].get<double>();
    if(test.contains("update"))options.updateMs=test["update"].get<double>();
    if(test.contains("retry"))options.retry.retries=test["retry"].get<int>();
    if(test.contains("retryOptions")){
      const auto& retry=test["retryOptions"];
      if(retry.contains("retries"))
        options.retry.retries=retry["retries"].get<int>();
      if(retry.contains("minTimeout"))
        options.retry.minTimeoutMs=retry["minTimeout"].get<double>();
      if(retry.contains("maxTimeout"))
        options.retry.maxTimeoutMs=retry["maxTimeout"].get<double>();
      if(retry.contains("factor"))
        options.retry.factor=retry["factor"].get<double>();
      if(retry.contains("randomize"))
        options.retry.randomize=retry["randomize"].get<bool>();
    }
    if(test.value("custom",false))
      options.lockfilePath=std::string(root)+"/custom.lock";
    const auto lockPath=options.lockfilePath.value_or(file+".lock");
    std::string compromised;
    options.onCompromised=[&](const locking::Error& error){
      compromised=error.code;};
    std::optional<locking::Release> release;
    std::function<void()> syncRelease;
    Json values=Json::array();
    for(const auto& action:test["actions"]){
      const auto op=action.get<std::string>();
      try{
        if(op=="lock"){
          release=await(loop,locks.lock(file,options));
          values.push_back("acquired");
        }else if(op=="tryLock"){
          auto extra=await(loop,locks.lock(file,options));
          (void)await(loop,extra());values.push_back("acquired");
        }else if(op=="release"){
          (void)await(loop,(*release)());values.push_back("released");
        }else if(op=="unlock"){
          (void)await(loop,locks.unlock(file,options));
          values.push_back("unlocked");
        }else if(op=="check")values.push_back(
          await(loop,locks.check(file,options)));
        else if(op=="lockSync"){
          syncRelease=locks.lockSync(file,options);
          values.push_back("acquired");
        }else if(op=="releaseSync"){
          syncRelease();values.push_back("released");
        }else if(op=="checkSync")values.push_back(
          locks.checkSync(file,options));
        else if(op=="createStale"||op=="createYoung"){
          if(::mkdir(lockPath.c_str(),0700)!=0)
            throw std::runtime_error("Cannot create lock directory");
          locking::file_ops::setMtime(lockPath,
            locking::file_ops::nowMs()-(op=="createStale"?100000:0));
          values.push_back("created");
        }else if(op=="waitHeartbeat"){
          const auto before=locking::file_ops::statMtime(lockPath);
          loop.setTimeout(1600,[](){});loop.run();
          values.push_back(locking::file_ops::statMtime(lockPath)!=before);
        }else if(op=="removeLock"){
          locking::file_ops::remove(lockPath);values.push_back("removed");
        }else if(op=="waitCompromised"){
          loop.setTimeout(1300,[](){});loop.run();
          values.push_back(compromised);
        }else throw std::runtime_error("Unknown lock operation");
      }catch(const locking::Error& error){
        values.push_back(Json{{"error",error.code}});
      }catch(const std::system_error& error){
        values.push_back(Json{{"error",error.code().value()==ENOENT
          ?"ENOENT":error.what()}});
      }catch(const std::exception& error){
        values.push_back(Json{{"error",error.what()}});
      }
    }
    try{if(release)(void)await(loop,(*release)());}catch(...){}
    try{if(syncRelease)syncRelease();}catch(...){}
    try{locking::file_ops::remove(lockPath);}catch(...){}
    (void)::rmdir(root);
    std::cout<<value::stringify(values)<<'\n';
  }
}
