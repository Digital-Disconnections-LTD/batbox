#include "attachments/files/FileAttachments.hpp"
#include "filesystem/FileOps.hpp"
#include "media/Media.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/IndigenousPath.hpp"
#include "gadgets/files/ActiveStash.hpp"
#include <filesystem>
#include <stdexcept>
#include <cerrno>
#ifndef _WIN32
#include <sys/stat.h>
#endif
#include <system_error>

namespace batbox::attachments::files {
namespace {
namespace fs=std::filesystem;
template<class T,class Work>
event_loop::Deferred<T> background(event_loop::Loop& loop,Work work){
  event_loop::Deferred<T> result(loop);
  runtime::contextThread([result,work=std::move(work)]()mutable{
    try{result.resolve(work());}
    catch(...){result.reject(std::current_exception());}
  }).detach();
  return result;
}
gadgets::GadgetCallContext gadgetContext(const execution::Context& context){
  gadgets::GadgetCallContext call;
  call.data=context.data;call.stopToken=context.stop;
  call.readFileState=context.readFileState;
  call.nestedMemoryAttachmentTriggers=context.nestedMemoryAttachmentTriggers;
  call.loadedNestedMemoryPaths=context.loadedNestedMemoryPaths;
  call.dynamicSkillDirTriggers=context.dynamicSkillDirTriggers;
  call.discoveredSkillNames=context.discoveredSkillNames;
  return call;
}
}
Services makeIndigenousFileServices(event_loop::Loop& loop,
    gadgets::files::FileReadServices& readServices,
    std::function<app_state::AppStateRef()> getAppState,
    std::function<bool(const std::string&,const app_state::AppStateRef&)>
      isFileReadDenied){
  if(!readServices.paths.getCwd||!getAppState||!isFileReadDenied)
    throw std::invalid_argument("Native file attachment app bindings required");
  Services services{loop};
  services.getAppState=std::move(getAppState);
  services.isFileReadDenied=std::move(isFileReadDenied);
  services.maxReadSizeBytes=readServices.defaultLimits.maxSizeBytes;
  services.expandPath=[paths=readServices.paths](const std::string& path){
    return filesystem::expandPath(path,paths);
  };
  services.displayPath=[cwd=readServices.paths.getCwd](
      const std::string& path){
    const auto relative=runtime::pathFromUtf8(path).lexically_relative(
      runtime::pathFromUtf8(cwd()));
    if(relative.empty())return path;
    const auto generic=relative.generic_u8string();
    return std::string(reinterpret_cast<const char*>(generic.data()),generic.size());
  };
  services.withinReadSizeLimit=[](const std::string& path,std::size_t limit){
    try{return fs::file_size(runtime::pathFromUtf8(path))<=limit;}
    catch(...){return false;}
  };
  services.stat=[&loop](const std::string& path){
    return background<Stat>(loop,[path]{
#ifdef _WIN32
      const auto indigenous=runtime::pathFromUtf8(path);
      const auto status=fs::status(indigenous);
      if(fs::is_directory(status))return Stat{0.0,true};
      return Stat{static_cast<double>(fs::file_size(indigenous)),false};
#else
      struct stat info{};
      if(::stat(path.c_str(),&info)!=0)
        throw std::system_error(errno,std::generic_category(),"stat");
      return Stat{static_cast<double>(info.st_size),S_ISDIR(info.st_mode)};
#endif
    });
  };
  services.readDirectory=[&loop](const std::string& path){
    return background<std::vector<std::string>>(loop,[path]{
      std::vector<std::string> names;
      for(const auto& entry:fs::directory_iterator(runtime::pathFromUtf8(path)))
        names.push_back(runtime::pathToUtf8(entry.path().filename()));
      return names;
    });
  };
  services.pdfPageCount=[&loop](const std::string& path){
    return background<std::optional<int>>(loop,[path]{
      return media::getPDFPageCount(path);
    });
  };
  services.modificationTime=[&loop](const std::string& path){
    return background<double>(loop,[path]{
      return filesystem::getFileModificationTime(path);
    });
  };
  services.validateRead=[&loop,&readServices](const Json& input,
      execution::Context&){
    event_loop::Deferred<gadgets::ValidationResult> result(loop);
    try{result.resolve(gadgets::files::validateReadInput(input,readServices));}
    catch(...){result.reject(std::current_exception());}
    return result;
  };
  services.callRead=[&loop,&readServices](const Json& input,
      execution::Context& context){
    event_loop::Deferred<gadgets::GadgetResult> result(loop);
    try{
      auto call=gadgetContext(context);
      auto active=gadgets::files::withActiveStash(readServices,call);
      result.resolve(gadgets::files::callRead(input,call,active));
    }catch(...){result.reject(std::current_exception());}
    return result;
  };
  return services;
}
} // namespace batbox::attachments::files
