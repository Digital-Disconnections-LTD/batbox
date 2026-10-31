#include "locking/ProperLock.hpp"
#include "locking/FileOps.hpp"
#include "locking/Bindings.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

template<class T>T await(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<T> promise){
  std::optional<T> value;
  std::exception_ptr failure;
  promise.then([&](const T* result,const std::exception_ptr& error){
    if(error)failure=error;else value=*result;
  });
  loop.run();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::runtime_error("Directory lock did not settle");
  return *value;
}
int main(){
  using namespace batbox;
  char temporary[]="/tmp/batbox-proper-lock-XXXXXX";
  const char* directory=::mkdtemp(temporary);
  assert(directory);
  event_loop::Loop loop;
  locking::Manager locks(loop);
  auto release=await(loop,locks.lock(directory));
  assert(locking::file_ops::check(directory,
    locking::file_ops::normalize({})));
  assert(await(loop,locks.check(directory)));
  try{(void)await(loop,locks.lock(directory));assert(false);}
  catch(const locking::Error& error){assert(error.code=="ELOCKED");}
  const auto child=::fork();
  assert(child>=0);
  if(child==0){
    try{
      event_loop::Loop otherLoop;
      locking::Manager other(otherLoop);
      (void)await(otherLoop,other.lock(directory));
      ::_exit(2);
    }catch(const locking::Error& error){
      ::_exit(error.code=="ELOCKED"?0:3);
    }catch(...){::_exit(4);}
  }
  int status=0;assert(::waitpid(child,&status,0)==child);
  assert(WIFEXITED(status)&&WEXITSTATUS(status)==0);
  (void)await(loop,release());
  assert(!await(loop,locks.check(directory)));
  try{(void)await(loop,release());assert(false);}
  catch(const locking::Error& error){assert(error.code=="ERELEASED");}
  const auto lockPath=std::string(directory)+".lock";
  assert(::mkdir(lockPath.c_str(),0700)==0);
  locking::file_ops::setMtime(lockPath,
    locking::file_ops::nowMs()-100000);
  auto stolen=await(loop,locks.lock(directory));
  (void)await(loop,stolen());
  locking::Options heartbeat;
  heartbeat.staleMs=2000;heartbeat.updateMs=1000;
  heartbeat.onCompromised=[](const locking::Error& error){
    throw std::runtime_error("Unexpected compromise: "+error.code);};
  auto heartbeatRelease=await(loop,locks.lock(directory,heartbeat));
  const auto initial=locking::file_ops::statMtime(lockPath);
  bool waited=false;
  loop.setTimeout(1600,[&](){waited=true;});
  loop.run();
  assert(waited&&locking::file_ops::statMtime(lockPath)!=initial);
  (void)await(loop,heartbeatRelease());
  std::string compromised;
  heartbeat.onCompromised=[&](const locking::Error& error){
    compromised=error.code;};
  auto compromisedRelease=await(loop,locks.lock(directory,heartbeat));
  locking::file_ops::remove(lockPath);
  loop.setTimeout(1300,[](){});
  loop.run();
  assert(compromised=="ECOMPROMISED");
  try{(void)await(loop,compromisedRelease());assert(false);}
  catch(const locking::Error& error){assert(error.code=="ERELEASED");}
  locking::Options custom;
  custom.realpath=false;
  custom.lockfilePath=std::string(directory)+"/custom.lock";
  auto syncRelease=locks.lockSync(std::string(directory)+"/missing",custom);
  assert(locks.checkSync(std::string(directory)+"/missing",custom));
  syncRelease();
  assert(!locks.checkSync(std::string(directory)+"/missing",custom));
  auto cleanup=locking::makeCleanupCallbacks(loop,locks);
  const auto marker=std::string(directory)+"/marker";
  (void)await(loop,cleanup.lock(marker));
  locking::Options markerOptions;markerOptions.realpath=false;
  assert(await(loop,locks.check(marker,markerOptions)));
  (void)await(loop,cleanup.unlock(marker));
  assert(!await(loop,locks.check(marker,markerOptions)));
  assert(::rmdir(directory)==0);
  std::cout<<"Native directory lock fixture passed\n";
}
