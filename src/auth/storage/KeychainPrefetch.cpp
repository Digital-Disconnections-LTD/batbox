#include "auth/storage/KeychainPrefetch.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth::storage {
KeychainPrefetch::KeychainPrefetch(event_loop::Loop& loop,
    std::shared_ptr<KeychainState> state,PrefetchServices services):
  loop_(loop),state_(std::move(state)),services_(std::move(services)){
  if(!state_||!services_.platform||!services_.bareMode||
      !services_.spawnSecurity||!services_.primeStash)
    throw std::invalid_argument("Missing keychain prefetch binding");
}
void KeychainPrefetch::start(){
  if(services_.platform()!="darwin"||pending_||services_.bareMode())return;
  struct Pending {
    std::optional<PrefetchResult> oauth,legacy;
    bool settled=false;
  };
  auto state=std::make_shared<Pending>();
  event_loop::Deferred<event_loop::Unit> output(loop_);
  pending_=output;
  const auto settle=[this,state,output](){
    if(state->settled||!state->oauth||!state->legacy)return;
    state->settled=true;
    try{
      if(!state->oauth->timedOut)
        services_.primeStash(state->oauth->stdoutText);
      if(!state->legacy->timedOut)
        legacy_=state->legacy->stdoutText;
      output.resolve({});
    }catch(...){output.reject(std::current_exception());}
  };
  try{
    const auto oauthName=state_->serviceName(
      state_->services().credentialServiceSuffix);
    const auto oauth=services_.spawnSecurity(oauthName);
    const auto legacyName=state_->serviceName();
    const auto legacy=services_.spawnSecurity(legacyName);
    oauth.then([state,settle,output](const PrefetchResult* result,
        const std::exception_ptr& error){
      if(error){state->settled=true;output.reject(error);return;}
      state->oauth=*result;settle();
    });
    legacy.then([state,settle,output](const PrefetchResult* result,
        const std::exception_ptr& error){
      if(error){state->settled=true;output.reject(error);return;}
      state->legacy=*result;settle();
    });
  }catch(...){state->settled=true;output.reject(std::current_exception());}
}
event_loop::Deferred<event_loop::Unit>
KeychainPrefetch::ensureCompleted(){
  if(pending_)return *pending_;
  event_loop::Deferred<event_loop::Unit> output(loop_);
  output.resolve({});return output;
}
std::optional<std::optional<std::string>>
KeychainPrefetch::legacyApiKeyResult()const{return legacy_;}
void KeychainPrefetch::clearLegacyApiKeyResult(){legacy_.reset();}
}
