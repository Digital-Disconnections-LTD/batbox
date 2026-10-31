#include "flights/ProjectDiscovery.hpp"
#include "runtime/PathSanitization.hpp"
#include "runtime/Unicode.hpp"
#include "shell/PosixPath.hpp"
namespace batbox::flights {
event_loop::Deferred<std::string> canonicalizePath(event_loop::Loop& loop,
  const ProjectDiscoveryServices& services,const std::string& path){
  event_loop::Deferred<std::string> result(loop);
  const auto fallback=[path,result]{
    try{result.resolve(value::normalizeNFC(path));}catch(...){result.reject(std::current_exception());}
  };
  try{services.realpath(path).then([result,fallback](const std::string* resolved,const auto& error){
    if(error){fallback();return;}
    try{result.resolve(value::normalizeNFC(*resolved));}catch(...){fallback();}
  });}catch(...){fallback();}
  return result;
}
event_loop::Deferred<std::optional<std::string>> findProjectDir(event_loop::Loop& loop,
  const ProjectDiscoveryServices& services,const std::shared_ptr<ProjectDirectories>& directories,
  const std::string& path){
  event_loop::Deferred<std::optional<std::string>> result(loop);
  const auto fallback=[services,directories,path,result]{
    try{
      const auto sanitized=directories->sanitizeProjectPath(path);
      if(sanitized.size()<=runtime::MAX_SANITIZED_LENGTH){result.resolve(std::nullopt);return;}
      const auto prefix=sanitized.substr(0,runtime::MAX_SANITIZED_LENGTH);
      const auto projects=directories->getProjectsDir();
      try{services.readdir(projects,true).then([result,prefix,projects](const auto* entries,const auto& error){
        if(error){result.resolve(std::nullopt);return;}
        for(const auto& entry:*entries)if(entry.directory&&entry.name.starts_with(prefix+"-")){
          result.resolve(shell::joinPosixPath(projects,entry.name));return;
        }
        result.resolve(std::nullopt);
      });}catch(...){result.resolve(std::nullopt);}
    }catch(...){result.reject(std::current_exception());}
  };
  try{
    const auto exact=directories->getProjectDirPortable(path);
    try{services.readdir(exact,false).then([result,exact,fallback](const auto*,const auto& error){
      if(error)fallback();else result.resolve(exact);
    });}catch(...){fallback();}
  }catch(...){result.reject(std::current_exception());}
  return result;
}
}
