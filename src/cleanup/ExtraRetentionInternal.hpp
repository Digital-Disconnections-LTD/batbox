#pragma once
#include "cleanup/ExtraRetention.hpp"
#include <stdexcept>

namespace batbox::cleanup::detail {
inline void require(const ExtraServices& services){
  if(!services.fs.readdir||!services.fs.statMtimeMs||!services.fs.unlink||
    !services.fs.rmdir||!services.fs.rmRecursiveForce||!services.getCutoffDateMs||
    !services.getProjectsDir||!services.getConfigHome||!services.joinPath||
    !services.logError||services.gadgetResultsSubdir.empty())
    throw std::invalid_argument("cleanup filesystem/settings imports required");
}
inline event_loop::Deferred<bool> unlinkIfOld(event_loop::Loop& loop,
  const std::string& path,double cutoff,const ExtraServices& services){
  event_loop::Deferred<bool> result(loop);
  try{services.fs.statMtimeMs(path).then([path,cutoff,services,result](
    const double* mtime,const std::exception_ptr& error){
    if(error){result.reject(error);return;}
    if(!(*mtime<cutoff)){result.resolve(false);return;}
    try{services.fs.unlink(path).then([result](const event_loop::Unit*,
      const std::exception_ptr& unlinkError){
      if(unlinkError)result.reject(unlinkError);else result.resolve(true);
    });}catch(...){result.reject(std::current_exception());}
  });}catch(...){result.reject(std::current_exception());}
  return result;
}
inline event_loop::Deferred<event_loop::Unit> tryRmdir(event_loop::Loop& loop,
  const std::string& dir,const ExtraServices& services){
  event_loop::Deferred<event_loop::Unit> result(loop);
  try{services.fs.rmdir(dir).then([result](const event_loop::Unit*,
    const std::exception_ptr&){result.resolve(event_loop::Unit{});});}
  catch(...){result.resolve(event_loop::Unit{});}
  return result;
}
} // namespace batbox::cleanup::detail
