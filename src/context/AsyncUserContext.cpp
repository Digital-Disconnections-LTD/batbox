#include "context/AsyncContextInternal.hpp"
#include "utils/envUtils.hpp"

namespace batbox::context {
event_loop::Deferred<Json> AsyncContextStash::getUserContext(const AsyncServices& services){
  checkContextOwner(state_->owner);
  if(state_->user)return *state_->user;
  event_loop::Deferred<Json> result(state_->owner);
  auto finish=[services,result](const MemoryFiles* files){
    try{
      std::optional<std::string> instructionText;
      if(files){
        if(!services.projectServices)throw std::invalid_argument("Missing project instruction services");
        instructionText=instructions::project::getBatboxMds(
          instructions::project::filterInjectedMemoryFiles(*files,*services.projectServices),*services.projectServices);
      }
      requireContextBinding(services.setStashedBatboxMdContent,"setCachedBatboxMdContent");
      services.setStashedBatboxMdContent(instructionText&&!instructionText->empty()?instructionText:std::nullopt);
      Json context=Json::object();
      if(instructionText&&!instructionText->empty())context["batboxMd"]=*instructionText;
      std::string date;
      if(services.localISODate)date=services.localISODate();
      else{
        const auto& override=value::property(services.environment,"BATBOX_OVERRIDE_DATE");
        date=getLocalISODate(override.is_string()?std::optional<std::string>(override.get<std::string>()):std::nullopt);
      }
      context["currentDate"]="Today's date is "+date+".";result.resolve(std::move(context));
    }catch(...){result.reject(std::current_exception());}
  };
  try{
    bool disable=isEnvTruthy(value::property(services.environment,"BATBOX_DISABLE_BATBOX_MDS"));
    if(!disable){
      requireContextBinding(services.bareMode,"bareMode");
      if(services.bareMode()){
        requireContextBinding(services.additionalInstructionDirectories,"additionalInstructionDirectories");
        disable=services.additionalInstructionDirectories().empty();
      }
    }
    if(disable)finish(nullptr);
    else{
      requireContextBinding(services.getMemoryFiles,"getMemoryFiles");
      services.getMemoryFiles().then([result,finish](const MemoryFiles* files,const std::exception_ptr& error){
        if(error)result.reject(error);else finish(files);
      });
    }
  }catch(...){result.reject(std::current_exception());}
  state_->user=result;return result;
}
}
