#include "paste_store/PasteStore.hpp"
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::paste_store {
namespace {
template<class T>void required(const T& callback){
  if(!callback)throw std::invalid_argument("Paste cleanup import is required");
}
struct Run {
  Run(std::function<double()> date,Services providers):
    result(*providers.loop),getCutoff(std::move(date)),
    services(std::move(providers)){}
  event_loop::Deferred<event_loop::Unit> result;
  std::function<double()> getCutoff;
  Services services;
  std::string directory;
  std::vector<std::string> files;
  std::size_t index=0;
  double cutoff=0;
};
void next(const std::shared_ptr<Run>& run){
  while(run->index<run->files.size()){
    const auto& candidate=run->files[run->index];
    if(candidate.size()>=4&&candidate.substr(candidate.size()-4)==".txt")break;
    ++run->index;
  }
  if(run->index>=run->files.size()){run->result.resolve({});return;}
  const auto file=run->files[run->index++];
  std::string path;
  try{
    required(run->services.join);
    path=run->services.join(run->directory,file);
  }catch(...){run->result.reject(std::current_exception());return;}
  try{
    required(run->services.stat);
    auto stat=run->services.stat(path);
    stat.then([run,path](const Json* info,const std::exception_ptr& error){
      if(error){next(run);return;}
      try{
        const auto& mtime=value::property(*info,"mtimeMs");
        if(mtime.is_number()&&mtime.get<double>()<run->cutoff){
          required(run->services.unlink);
          auto unlink=run->services.unlink(path);
          unlink.then([run,path](const event_loop::Unit*,
              const std::exception_ptr& failure){
            if(!failure){
              try{
                required(run->services.logForDebugging);
                run->services.logForDebugging("Cleaned up old paste: "+path);
              }catch(...){/* Ignore per-file cleanup errors. */}
            }
            next(run);
          });
          return;
        }
      }catch(...){/* Ignore per-file cleanup errors. */}
      next(run);
    });
  }catch(...){next(run);}
}
}
event_loop::Deferred<event_loop::Unit> cleanupOldPastes(
    const std::function<double()>& cutoffDateGetTime,const Services& services){
  if(!services.loop)throw std::invalid_argument("Paste store owner loop required");
  auto run=std::make_shared<Run>(cutoffDateGetTime,services);
  try{
    run->directory=getPasteStoreDir(run->services);
  }catch(...){run->result.reject(std::current_exception());return run->result;}
  try{
    required(run->services.readdir);
    auto read=run->services.readdir(run->directory);
    read.then([run](const std::vector<std::string>* files,
        const std::exception_ptr& error){
      if(error){run->result.resolve({});return;}
      try{
        required(run->getCutoff);
        run->cutoff=run->getCutoff();
        run->files=*files;
        next(run);
      }catch(...){run->result.reject(std::current_exception());}
    });
  }catch(...){run->result.resolve({});}
  return run->result;
}
event_loop::Deferred<event_loop::Unit> cleanupOldPastes(
    double cutoffTimeMs,const Services& services){
  return cleanupOldPastes([cutoffTimeMs]{return cutoffTimeMs;},services);
}
}
