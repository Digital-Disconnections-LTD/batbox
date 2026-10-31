#include "context/AsyncContextInternal.hpp"
#include "utils/envUtils.hpp"

namespace batbox::context {
event_loop::Deferred<Json> AsyncContextStash::getSystemContext(const AsyncServices& services){
  checkContextOwner(state_->owner);
  if(state_->system)return *state_->system;
  event_loop::Deferred<Json> result(state_->owner);
  auto finish=[injectionState=state_->injection,services,result](const std::optional<std::string>& git){
    try{
      requireContextBinding(services.stashBreakerFeature,"cacheBreakerFeature");
      const auto injection=services.stashBreakerFeature()?*injectionState:std::nullopt;
      Json context=Json::object();
      if(git&&!git->empty())context["gitStatus"]=*git;
      if(services.stashBreakerFeature()&&injection&&!injection->empty())
        context["cacheBreaker"]="[CACHE_BREAKER: "+*injection+"]";
      result.resolve(std::move(context));
    }catch(...){result.reject(std::current_exception());}
  };
  try{
    bool skip=isEnvTruthy(value::property(services.environment,"BATBOX_REMOTE"));
    if(!skip){requireContextBinding(services.includeGitInstructions,"includeGitInstructions");skip=!services.includeGitInstructions();}
    if(skip)finish(std::nullopt);
    else getGitStatus(services).then([result,finish](const std::optional<std::string>* git,const std::exception_ptr& error){
      if(error)result.reject(error);else finish(*git);
    });
  }catch(...){result.reject(std::current_exception());}
  state_->system=result;return result;
}
}
