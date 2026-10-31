#include "auth/refresh/Invalidate.hpp"
#include <cassert>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main(){
  using namespace batbox;
  char temporary[]="/tmp/batbox-auth-invalidate-XXXXXX";
  const char* home=::mkdtemp(temporary);
  assert(home);
  const auto path=std::string(home)+"/.batbox-credentials.json";
  event_loop::Loop loop;
  int full=0,memo=0;
  auth::refresh::InvalidateServices services;
  services.configHome=[&](){return std::string(home);};
  services.statMtimeMs=[&](const std::string& file){
    return auth::refresh::indigenousStatMtimeMs(loop,file);};
  services.clearFullStash=[&](){++full;};
  services.clearMemoOnly=[&](){++memo;};
  auth::refresh::DiskInvalidation invalidation(loop,services);
  const auto invalidate=[&](){
    bool done=false;
    invalidation.invalidateIfDiskChanged().then([&](
        const event_loop::Unit*,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      done=true;
    });
    loop.run();
    if(!done)throw std::runtime_error("Native invalidation did not settle");
  };
  invalidate();assert(full==0&&memo==1);
  const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
  assert(fd>=0);assert(::close(fd)==0);
  timespec timestamps[2]={{100,0},{100,0}};
  assert(::utimensat(AT_FDCWD,path.c_str(),timestamps,0)==0);
  invalidate();assert(full==1&&memo==1);
  invalidate();assert(full==1&&memo==1);
  timestamps[1].tv_sec=101;
  assert(::utimensat(AT_FDCWD,path.c_str(),timestamps,0)==0);
  invalidate();assert(full==2&&memo==1);
  assert(::unlink(path.c_str())==0);
  invalidate();assert(full==2&&memo==2);
  assert(::rmdir(home)==0);
  std::cout<<"Native OAuth credential mtime fixture passed\n";
}
