#include "auth/refresh/Invalidate.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth::refresh {
DiskInvalidation::DiskInvalidation(event_loop::Loop& loop,
    InvalidateServices services):loop_(loop),services_(std::move(services)){
  if(!services_.configHome||!services_.statMtimeMs||
      !services_.clearFullStash||!services_.clearMemoOnly||
      services_.credentialsFile.empty())
    throw std::invalid_argument("Missing OAuth disk invalidation binding");
}
event_loop::Deferred<event_loop::Unit>
DiskInvalidation::invalidateIfDiskChanged(){
  event_loop::Deferred<event_loop::Unit> output(loop_);
  try{
    const auto home=services_.configHome();
    const auto path=home+(home.empty()||home.back()=='/'?"":"/")+
      services_.credentialsFile;
    services_.statMtimeMs(path).then([this,output](
        const double* mtime,const std::exception_ptr& error){
      if(error){
        try{services_.clearMemoOnly();output.resolve({});}
        catch(...){output.reject(std::current_exception());}
        return;
      }
      try{
        if(*mtime!=lastMtimeMs_){
          lastMtimeMs_=*mtime;
          services_.clearFullStash();
        }
        output.resolve({});
      }catch(...){
        try{services_.clearMemoOnly();output.resolve({});}
        catch(...){output.reject(std::current_exception());}
      }
    });
  }catch(...){
    try{services_.clearMemoOnly();output.resolve({});}
    catch(...){output.reject(std::current_exception());}
  }
  return output;
}
}
