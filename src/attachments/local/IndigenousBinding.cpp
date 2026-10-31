#include "attachments/local/LocalCollectors.hpp"
#include "runtime/AsyncLaunch.hpp"
#include <stdexcept>

namespace batbox::attachments::local {
Services makeIndigenousLocalServices(event_loop::Loop& loop,
    std::shared_ptr<files::Services> fileServices,
    const gadgets::files::FileReadServices& readServices){
  if(!fileServices||!fileServices->getAppState)
    throw std::invalid_argument("Native changed-file services required");
  Services services(loop);
  services.files=std::move(fileServices);
  services.getAppState=services.files->getAppState;
  services.readImageWithTokenBudget=[&loop,image=readServices.image,
      limit=readServices.defaultLimits.maxTokens](const std::string& path){
    event_loop::Deferred<Json> output(loop);
    runtime::contextThread([output,path,image,limit]{
      try{output.resolve(gadgets::files::readImageWithTokenBudget(
        path,limit,std::nullopt,image));}
      catch(...){output.reject(std::current_exception());}
    }).detach();
    return output;
  };
  return services;
}
} // namespace batbox::attachments::local
