#include "instructions/project/async/ExternalWarning.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::instructions::project::async {
Deferred<bool> shouldShowExternalIncludesWarning(event_loop::Loop& loop,
  const ExternalWarningImports& imports){
  if(!loop.isOwnerThread())throw std::logic_error("External instruction warning owner required");
  Deferred<bool> output(loop);
  try{
    if(!imports.getCurrentProjectConfig||!imports.getForcedMemoryFiles||
      !imports.hasExternalIncludes)
      throw std::invalid_argument("External warning imports required");
    const auto config=imports.getCurrentProjectConfig();
    if(config.approved||config.warningShown){output.resolve(false);return output;}
    imports.getForcedMemoryFiles().then([output,contains=imports.hasExternalIncludes](
      const MemoryFiles* files,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{output.resolve(contains(*files));}
      catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
ExternalWarningImports bindExternalWarningImports(MemoryDiscoveryStash& stash,
  Services services,std::function<ExternalIncludeConfig()> config){
  ExternalWarningImports imports;
  imports.getCurrentProjectConfig=std::move(config);
  imports.getForcedMemoryFiles=[&stash,services]{return stash.getMemoryFiles(true,services);};
  imports.hasExternalIncludes=[sync=services.sync](const MemoryFiles& files){
    return hasExternalBatboxMdIncludes(files,sync);};
  return imports;
}
} // namespace batbox::instructions::project::async
